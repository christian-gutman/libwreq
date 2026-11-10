#ifndef CORE_REQUEST_H
#define CORE_REQUEST_H
 


#include "core.h"
#include "core_io.h"
#include "core_encoding.h"
#include "core_string.h"
#include "core_crypto.h"

#ifdef __cplusplus
extern "C"
{
#endif

#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdbool.h>
 
#pragma comment(lib, "winhttp.lib")

#define MAX_GLOBAL_CONNECTIONS 1024

    // 1. REGISTRO GLOBAL Y CANDADO CON GUARDA ATÓMICA
    static HINTERNET g_active_connections[MAX_GLOBAL_CONNECTIONS] = {NULL};
    static CRITICAL_SECTION g_registry_lock;
    static volatile LONG g_lock_initialized = 0; // Guarda atómica para evitar el error 183

    CORE_API BOOL WDisconnect(HINTERNET *phHandle);
    // extern BOOL WDisconnect(HINTERNET *phHandle);

    // Función interna thread-safe que asegura una ÚNICA inicialización en la historia del proceso
    static void InitRegistryLockIfNeeded(void)
    {
        // InterlockedCompareExchange es una instrucción directa del procesador (atómica).
        // Si g_lock_initialized es 0, lo cambia a 1 y devuelve 0. Solo un hilo ganará esta carrera.
        if (InterlockedCompareExchange(&g_lock_initialized, 1, 0) == 0)
        {
            InitializeCriticalSection(&g_registry_lock);
        }
    }

    // Función auxiliar interna para registrar un handle (Se llama adentro de WConnect)
    static void RegisterHandleForAutoCleanup(HINTERNET h)
    {
        InitRegistryLockIfNeeded(); // Nos aseguramos de que el candado exista de forma segura

        EnterCriticalSection(&g_registry_lock);
        for (int i = 0; i < MAX_GLOBAL_CONNECTIONS; i++)
        {
            if (g_active_connections[i] == NULL)
            {
                g_active_connections[i] = h;
                break;
            }
        }
        LeaveCriticalSection(&g_registry_lock);
    }

    // Función auxiliar interna para des-registrar un handle (Se llama adentro de WDisconnect)
    static void UnregisterHandleFromAutoCleanup(HINTERNET h)
    {
        // Si el candado nunca se inicializó, significa que no hay nada registrado
        if (InterlockedOr(&g_lock_initialized, 0) == 0)
            return;

        EnterCriticalSection(&g_registry_lock);
        for (int i = 0; i < MAX_GLOBAL_CONNECTIONS; i++)
        {
            if (g_active_connections[i] == h)
            {
                g_active_connections[i] = NULL;
                break;
            }
        }
        LeaveCriticalSection(&g_registry_lock);
    }

    // PURGA AUTOMÁTICA DE EMERGENCIA EXTERNALIZADA
    // Esta función la podés exportar o llamar al cerrar tu sistema principal
    static void WGlobalCleanup(void)
    {
        if (InterlockedCompareExchange(&g_lock_initialized, 2, 1) == 1)
        {
            EnterCriticalSection(&g_registry_lock);
            for (int i = 0; i < MAX_GLOBAL_CONNECTIONS; i++)
            {
                if (g_active_connections[i] != NULL)
                {
                   // HINTERNET hToClean = g_active_connections[i];
                    // Ejecutamos tu desconexión original pasándole la dirección de la copia
                    //WDisconnect(&hToClean);
                    WDisconnect(&g_active_connections[i]);
                }
            }
            LeaveCriticalSection(&g_registry_lock);
            DeleteCriticalSection(&g_registry_lock);
        }
    }
    // Función auxiliar interna para actualizar el array desde el Callback (100% Auto-inicializada)
    static void UpdateHandleInAutoCleanup(HINTERNET hOld, HINTERNET hNew)
    {
        // Si por una anomalía extrema el candado no existe, lo creamos en el acto de forma atómica
        InitRegistryLockIfNeeded();

        EnterCriticalSection(&g_registry_lock);
        for (int i = 0; i < MAX_GLOBAL_CONNECTIONS; i++)
        {
            if (g_active_connections[i] == hOld)
            {
                g_active_connections[i] = hNew; // Intercambio seguro en el array global
                break;
            }
        }
        LeaveCriticalSection(&g_registry_lock);
    }
    /*
        #define MAX_GLOBAL_CONNECTIONS 1024

        static HINTERNET g_active_connections[MAX_GLOBAL_CONNECTIONS] = {NULL};
        static CRITICAL_SECTION g_registry_lock;
        static BOOL g_registry_initialized = TRUE;

        static void RegisterHandleForAutoCleanup(HINTERNET h)
        {
            if (!g_registry_initialized)
            {
                InitializeCriticalSection(&g_registry_lock);
                g_registry_initialized = TRUE;
            }

            EnterCriticalSection(&g_registry_lock);
            for (int i = 0; i < MAX_GLOBAL_CONNECTIONS; i++)
            {
                if (g_active_connections[i] == NULL)
                {
                    g_active_connections[i] = h;
                    break;
                }
            }
            LeaveCriticalSection(&g_registry_lock);
        }

        static void UnregisterHandleFromAutoCleanup(HINTERNET h)
        {
            if (!g_registry_initialized)
                return;

            EnterCriticalSection(&g_registry_lock);
            for (int i = 0; i < MAX_GLOBAL_CONNECTIONS; i++)
            {
                if (g_active_connections[i] == h)
                {
                    g_active_connections[i] = NULL;
                    break;
                }
            }
            LeaveCriticalSection(&g_registry_lock);
        }
        static void UpdateHandleInAutoCleanup(HINTERNET hOld, HINTERNET hNew)
        {
            EnterCriticalSection(&g_registry_lock);
            for (int i = 0; i < MAX_GLOBAL_CONNECTIONS; i++)
            {
                if (g_active_connections[i] == hOld)
                {
                    g_active_connections[i] = hNew; // Reemplazo directo en O(N) bajo el candado
                    break;
                }
            }
            LeaveCriticalSection(&g_registry_lock);
        }
    */
    static __forceinline void HandleToStrLog(
        HINTERNET *pHandle,
        BOOL bSuccess,
        const char *Msg)
    {
        char szLogBuffer[512];
        DWORD dwErr = GetLastError();
        HINTERNET hActualValue = (pHandle) ? *pHandle : NULL;
        _snprintf_s(szLogBuffer, sizeof(szLogBuffer), _TRUNCATE,
                    "[LOG] %s | Success: %s | Handle: %p | GetLastError: %lu",
                    Msg ? Msg : "WinHTTP Event",
                    bSuccess ? "TRUE" : "FALSE",
                    hActualValue,
                    dwErr);

        wchar_t wLogPath[MAX_PATH];
        if (GetPathInLibraryFolder(wLogPath, MAX_PATH, L"vfputils.log"))
        {
            wchar_t *wLogMessage = StrAutoToUnicode(szLogBuffer, (int)strlen(szLogBuffer), NULL);
            if (wLogMessage)
            {
                WriteTextLogWithDate(wLogPath, wLogMessage);
                free(wLogMessage);
            }
        }
    }
    static __forceinline DWORD TlsProtocol(int tlsVersion)
    {
        DWORD dwProtocols = 0;
        switch (tlsVersion)
        {
        case 1:
            dwProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1;
            break;
        case 2:
            dwProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1;
            break;
        case 3:
            dwProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
            break;
        case 4:
            dwProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
            break;
        case 5:
        default:
            dwProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
            break;
        }
        return dwProtocols;
    }

    static __forceinline BOOL WConfigureSslSecurity(
        HINTERNET hRequest,
        BOOL bIgnoreSslErrors,
        const char *szClientCertSubject)
    {
        if (!hRequest)
            return FALSE;

        if (bIgnoreSslErrors)
        {
            DWORD dwFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                            SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE |
                            SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                            SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;

            if (!WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &dwFlags, sizeof(dwFlags)))
            {
                return FALSE;
            }
        }
        if (szClientCertSubject && szClientCertSubject[0] != '\0')
        {

            wchar_t *wClientCertSubjectLocal = StrAutoToUnicode(szClientCertSubject, (int)strlen(szClientCertSubject), NULL);
            HCERTSTORE hMyStore = CertOpenSystemStoreW(0, L"MY");
            if (hMyStore)
            {
                PCCERT_CONTEXT pCertContext = CertFindCertificateInStore(
                    hMyStore,
                    X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                    0,
                    CERT_FIND_SUBJECT_STR_W,
                    wClientCertSubjectLocal,
                    NULL);
                free(wClientCertSubjectLocal);

                if (pCertContext)
                {
                    BOOL bCertSet = WinHttpSetOption(hRequest, WINHTTP_OPTION_CLIENT_CERT_CONTEXT, (LPVOID)pCertContext, sizeof(PCCERT_CONTEXT));

                    CertFreeCertificateContext(pCertContext);
                    CertCloseStore(hMyStore, 0);

                    if (!bCertSet)
                        return FALSE;
                }
                else
                {
                    CertCloseStore(hMyStore, 0);
                    return FALSE;
                }
            }
            else
            {
                free(wClientCertSubjectLocal);
                return FALSE;
            }
        }
        return TRUE;
    }

    static __forceinline char *WQueryResponseHeader(HINTERNET hRequest, const char *szHeaderName)
    {
        if (!hRequest)
            return NULL;

        DWORD dwSize = 0;
        // 1. Convertimos el nombre de la cabecera de UTF-8 a Unicode para Windows
        wchar_t *wHeaderParam = (szHeaderName && szHeaderName[0] != '\0') ? StrAutoToUnicode(szHeaderName, -1, NULL) : NULL;
        DWORD dwQueryFlags = wHeaderParam ? WINHTTP_QUERY_CUSTOM : WINHTTP_QUERY_RAW_HEADERS_CRLF;

        BOOL bPreCheck = WinHttpQueryHeaders(hRequest, dwQueryFlags, wHeaderParam, WINHTTP_NO_OUTPUT_BUFFER, &dwSize, WINHTTP_NO_HEADER_INDEX);
        if (wHeaderParam)
            free(wHeaderParam);

        if (!bPreCheck && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            return NULL;
        if (dwSize == 0)
            return NULL;

        wchar_t *wBuffer = (wchar_t *)malloc(dwSize);
        if (wBuffer)
        {
            if (WinHttpQueryHeaders(hRequest, dwQueryFlags, wHeaderParam, wBuffer, &dwSize, WINHTTP_NO_HEADER_INDEX))
            {
                // char *szResult = StrUnicodeToAuto(wBuffer, (int)(dwSize / sizeof(wchar_t)), NULL);
                char *szResult = UnicodeToUtf8(wBuffer, (int)(dwSize / sizeof(wchar_t)), NULL);
                free(wBuffer);
                return szResult;
            }
            free(wBuffer);
        }
        return NULL;
    }
    static __forceinline void GetMimeTypeFromExtension(const char *szFileName, char *szOutMime, DWORD dwOutSize)
    {
        strcpy_s(szOutMime, dwOutSize, "application/octet-stream");
        if (!szFileName)
            return;
        const char *szExt = strrchr(szFileName, '.');
        if (szExt)
        {
            char szMime[128] = {0};
            DWORD dwSize = sizeof(szMime);
            // 3. Consultamos al registro de Windows (HKEY_CLASSES_ROOT\.ext -> Content Type)
            // Usamos RegOpenKeyExA y RegQueryValueExA que son compatibles con cualquier versión de Windows
            HKEY hKey;
            if (RegOpenKeyExA(HKEY_CLASSES_ROOT, szExt, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
            {
                if (RegQueryValueExA(hKey, "Content Type", NULL, NULL, (LPBYTE)szMime, &dwSize) == ERROR_SUCCESS)
                {
                    if (szMime[0] != '\0')
                    {
                        strcpy_s(szOutMime, dwOutSize, szMime);
                    }
                }
                RegCloseKey(hKey);
            }
        }
    }

    typedef struct _WUrl
    {
        char *host;
        char *path;
        INTERNET_PORT port;
        BOOL isSecure;
        BOOL isValid;
    } WUrl;

    CORE_API WUrl WParseUrl(const char *szUrl);

    CORE_API HINTERNET WConnect(
        const char *wAgent,
        const char *wHost,
        INTERNET_PORT port,
        const char *wProxyUrl);


    CORE_API BOOL WDisconnect(HINTERNET *phHandle);

    
    CORE_API BOOL WRequest(
        HINTERNET hConnect, const char *wVerb, const char *wPath, int tlsVersion, BOOL isSecure,
        BOOL bIgnoreSslErrors, const char *wClientCertSubject,
        const char *headers, const char *contentType, const char *boundary,
        const char *textBody, const char *filePath,
        char **ppOutResponseBuffer, HINTERNET *phOutRequest, DWORD *pStatusCode);

    CORE_API BOOL WCloseRequest(HINTERNET *pHandleRequest);

#ifdef __cplusplus
}
#endif

#endif // CORE_REQUEST_H

