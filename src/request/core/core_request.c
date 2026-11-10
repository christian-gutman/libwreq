#include "core_request.h"

/*
// ============================================================================
// EL GANCHO AUTÓNOMO DEL SISTEMA OPERATIVO (DllMain para DLL y FLL)
// ============================================================================
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    // Silenciamos parámetros nativos que no usamos para evitar warnings C4100
    UNREFERENCED_PARAMETER(hinstDLL);
    UNREFERENCED_PARAMETER(lpvReserved);

    // Evento nativo: El proceso principal se está cerrando o descargando la librería de la RAM
    if (fdwReason == DLL_PROCESS_DETACH) {
        
        // Verificamos de forma atómica si la librería fue utilizada alguna vez.
        // Si g_lock_initialized vale 1 (inicializada), cambiamos el estado a 2 (apagado).
        // Esto blinda el cierre y asegura que la purga se ejecute UNA sola vez.
        if (InterlockedCompareExchange(&g_lock_initialized, 2, 1) == 1) {
            
            // Forzamos un log directo a tu archivo para verificar que Windows entró acá
            // (Podés usar tu HandleToStrLog pasándole NULL o un indicador de sistema)
            printf("[🧹 DLL_PROCESS_DETACH] La aplicación se cerró. Iniciando purga automática de handles...\n");

            EnterCriticalSection(&g_registry_lock);
            
            int auto_cleaned_count = 0;
            for (int i = 0; i < MAX_GLOBAL_CONNECTIONS; i++) {
                if (g_active_connections[i] != NULL) {
                    HINTERNET hToClean = g_active_connections[i];
                    
                    // LLAMADA EXCLUSIVA: Ejecuta tu WDisconnect original que frena hilos,
                    // libera el malloc de la RAM y cierra los sockets de Microsoft
                    BOOL bClean = WDisconnect(&hToClean);
                    if (bClean) {
                        auto_cleaned_count++;
                    }
                }
            }
            
            LeaveCriticalSection(&g_registry_lock);
            
            // Destrucción final física del candado de la lista global
            DeleteCriticalSection(&g_registry_lock);
            
            printf("[✨ AUTO-CLEAN SUCCESS] Se liberaron de la RAM %d conexiones huérfanas de forma autónoma.\n", auto_cleaned_count);
        }
    }
    
    return TRUE; // Indicar a Windows que el evento se procesó con éxito
}
 */
WUrl WParseUrl(const char *szUrl)
{
    WUrl out = {0};
    out.isValid = FALSE;
    if (!szUrl || szUrl == '\0')
        return out;

    wchar_t *wUrl = StrAutoToUnicode(szUrl, -1, NULL);
    if (!wUrl)
        return out;

    URL_COMPONENTS urlComp = {0};
    urlComp.dwStructSize = sizeof(URL_COMPONENTS);
    urlComp.dwHostNameLength = (DWORD)-1;
    urlComp.dwUrlPathLength = (DWORD)-1;

    if (!WinHttpCrackUrl(wUrl, (DWORD)wcslen(wUrl), 0, &urlComp))
    {
        free(wUrl);
        return out;
    }

    if (urlComp.dwHostNameLength > 0)
    {
        //    out.host = StrUnicodeToAuto(urlComp.lpszHostName, urlComp.dwHostNameLength, NULL);

        out.host = UnicodeToUtf8(urlComp.lpszHostName, urlComp.dwHostNameLength, NULL);
    }
    if (urlComp.dwUrlPathLength > 0)
    {
        //    out.path = StrUnicodeToAuto(urlComp.lpszUrlPath, urlComp.dwUrlPathLength, NULL);

        out.path = UnicodeToUtf8(urlComp.lpszUrlPath, urlComp.dwUrlPathLength, NULL);
    }
    else
    {
        out.path = _strdup("/");
    }

    out.isSecure = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
    out.port = (urlComp.nPort == 0 || urlComp.nPort == INTERNET_DEFAULT_PORT) ? (out.isSecure ? 443 : 80) : (INTERNET_PORT)urlComp.nPort;
    out.isValid = (out.host && out.path);

    free(wUrl);
    return out;
}

typedef struct
{
    HINTERNET hSession;
    HINTERNET hConnect;
    wchar_t wHost[256];
    INTERNET_PORT port;
    HANDLE hHeartbeatThread;
    CRITICAL_SECTION lock;
    BOOL is_running;
} WInternalConnectionContext;

static DWORD WINAPI WInternalHeartbeatThread(LPVOID lpParam)
{
    WInternalConnectionContext *ctx = (WInternalConnectionContext *)lpParam;
    if (!ctx)
        return 1;

    DWORD elapsed = 0;
    DWORD interval_ms = 180000; // Dispara un OPTIONS cada 3 minutos

    while (ctx->is_running)
    {
        Sleep(1000); // Revisa cada 1 segundo si debe apagarse
        elapsed += 1000;

        if (elapsed >= interval_ms)
        {
            elapsed = 0;

            EnterCriticalSection(&ctx->lock);
            HINTERNET hActiveConnect = ctx->hConnect;

            if (!hActiveConnect || !ctx->is_running)
            {
                LeaveCriticalSection(&ctx->lock);
                continue;
            }

            HINTERNET hHeartbeatReq = WinHttpOpenRequest(ctx->hConnect, L"OPTIONS", L"/",
                                                         NULL, WINHTTP_NO_REFERER,
                                                         WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);

            LeaveCriticalSection(&ctx->lock);
            if (hHeartbeatReq)
            {
                HandleToStrLog(&ctx->hConnect, (ctx->hConnect != NULL), "HEARTBEAT - hConnect");
                if (WinHttpSendRequest(hHeartbeatReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
                {
                    WinHttpReceiveResponse(hHeartbeatReq, NULL);
                }
                WinHttpCloseHandle(hHeartbeatReq);
            }
        }
    }
    return 0;
}
/*
static void CALLBACK WInternalNetworkCallback(
    HINTERNET hInternet,
    DWORD_PTR dwContext,
    DWORD dwInternetStatus,
    LPVOID lpvStatusInformation,
    DWORD dwStatusInformationLength)
{
    UNREFERENCED_PARAMETER(hInternet);
    UNREFERENCED_PARAMETER(lpvStatusInformation);
    UNREFERENCED_PARAMETER(dwStatusInformationLength);

    WInternalConnectionContext *ctx = (WInternalConnectionContext *)dwContext;

    if (!ctx)
        return;

    if (!ctx->is_running)
        return;

    if (dwInternetStatus == WINHTTP_CALLBACK_STATUS_CONNECTION_CLOSED)
    {
        HandleToStrLog(&ctx->hSession, (ctx->hSession != NULL), " Socket Closed Repair New Thread Callback");
        EnterCriticalSection(&ctx->lock);
        if (!ctx->is_running)
        {
            LeaveCriticalSection(&ctx->lock);
            return;
        }

        if (ctx->hConnect)
        {
            BOOL bClose = WinHttpCloseHandle(ctx->hConnect);
            HandleToStrLog(&ctx->hConnect, bClose, " Old hConnect Close - InternalNetworkCallback");
            ctx->hConnect = NULL;
        }

        HINTERNET hNewConnect = WinHttpConnect(ctx->hSession, ctx->wHost, ctx->port, 0);
        if (hNewConnect)
        {
            ctx->hConnect = hNewConnect;
            HandleToStrLog(&hNewConnect, TRUE, " New hConnect - New Thread Callback Success ");
        }
        else
        {
            DWORD err = GetLastError();
            char errorMsgBuffer[128];
            snprintf(errorMsgBuffer, sizeof(errorMsgBuffer),
                     " hSession - New hConnect - New Thread Callback FAILED | Windows Error Code: %lu",
                     err);

            HandleToStrLog(&ctx->hSession, FALSE, errorMsgBuffer);
        }
        LeaveCriticalSection(&ctx->lock);
    }
}*/
static void CALLBACK WInternalNetworkCallback(
    HINTERNET hInternet,
    DWORD_PTR dwContext,
    DWORD dwInternetStatus,
    LPVOID lpvStatusInformation,
    DWORD dwStatusInformationLength) 
{
    UNREFERENCED_PARAMETER(hInternet);
    UNREFERENCED_PARAMETER(lpvStatusInformation);
    UNREFERENCED_PARAMETER(dwStatusInformationLength);

    WInternalConnectionContext *ctx = (WInternalConnectionContext *)dwContext;
    if (!ctx) return;
    if (!ctx->is_running) return;

    if (dwInternetStatus == WINHTTP_CALLBACK_STATUS_CONNECTION_CLOSED) {
        HandleToStrLog(&ctx->hSession, (ctx->hSession != NULL), " Socket Closed Repair New Thread Callback");

 
        EnterCriticalSection(&ctx->lock);
        if (!ctx->is_running) {
            LeaveCriticalSection(&ctx->lock);
            return;
        }
        HINTERNET hOldConnect = ctx->hConnect;
        ctx->hConnect = NULL; 
        LeaveCriticalSection(&ctx->lock); 

        if (hOldConnect) {
            BOOL bClose = WinHttpCloseHandle(hOldConnect);
            HandleToStrLog(&hOldConnect, bClose, " Old hConnect Close - InternalNetworkCallback");
        }
 
        HINTERNET hNewConnect = WinHttpConnect(ctx->hSession, ctx->wHost, ctx->port, 0);
 
        EnterCriticalSection(&ctx->lock);
        if (ctx->is_running && hNewConnect) {
            UpdateHandleInAutoCleanup(hOldConnect, hNewConnect);
            ctx->hConnect = hNewConnect;
            HandleToStrLog(&hNewConnect, TRUE, " New hConnect - New Thread Callback Success ");
        } 
        else if (hNewConnect) {
            BOOL bnew =   WinHttpCloseHandle(hNewConnect);
            HandleToStrLog(&hNewConnect, bnew, "New hConnect - Close hConnect SafeGuard");
        }
        else {
            DWORD err = GetLastError();
            char errorMsgBuffer[128];
            snprintf(errorMsgBuffer, sizeof(errorMsgBuffer), 
                     " hSession - New hConnect - New Thread Callback FAILED | Windows Error Code: %lu", err);
            HandleToStrLog(&ctx->hSession, FALSE, errorMsgBuffer);
        }
        LeaveCriticalSection(&ctx->lock);
    }
}

HINTERNET WConnect(
    const char *szAgent,
    const char *szHost,
    INTERNET_PORT port,
    const char *szProxyUrl)
{
    if (!szHost || szHost[0] == '\0')
    {
        return NULL;
    }

    wchar_t *wAgent = (szAgent && szAgent[0] != '\0') ? StrAutoToUnicode(szAgent, -1, NULL) : NULL;
    wchar_t *wHost = StrAutoToUnicode(szHost, -1, NULL);
    wchar_t *wProxy = (szProxyUrl && szProxyUrl[0] != '\0') ? StrAutoToUnicode(szProxyUrl, -1, NULL) : NULL;

    HINTERNET hSession = WinHttpOpen(
        wAgent ? wAgent : L"Universal Http Agent Sync/Async 4.2",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);

    if (wAgent)
        free(wAgent);

    if (!hSession)
    {
        if (wHost)
            free(wHost);
        if (wProxy)
            free(wProxy);
        return NULL;
    }
    HandleToStrLog(&hSession, (hSession != NULL), "WConnect - Open hSession");

    if (wProxy)
    {
        WINHTTP_PROXY_INFO proxyInfo = {WINHTTP_ACCESS_TYPE_NAMED_PROXY, wProxy, WINHTTP_NO_PROXY_BYPASS};
        WinHttpSetOption(hSession, WINHTTP_OPTION_PROXY, &proxyInfo, sizeof(proxyInfo));
    }
    else
    {
        WINHTTP_PROXY_INFO proxyInfo = {WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS};
        WinHttpSetOption(hSession, WINHTTP_OPTION_PROXY, &proxyInfo, sizeof(proxyInfo));
    }

    HINTERNET hConnect = WinHttpConnect(
        hSession,
        wHost,
        (port == 0) ? INTERNET_DEFAULT_PORT : port,
        0);

    HandleToStrLog(&hConnect, (hConnect != NULL), "WConnect - Open hConnect");

    if (wProxy)
        free(wProxy);

    if (wHost)
        free(wHost);

    if (hConnect)
    {
        RegisterHandleForAutoCleanup(hConnect);
        WInternalConnectionContext *ctx = (WInternalConnectionContext *)malloc(sizeof(WInternalConnectionContext));
        if (ctx)
        {
            ctx->hSession = hSession;
            ctx->hConnect = hConnect;
            ctx->port = (port == 0) ? INTERNET_DEFAULT_PORT : port;
            wcscpy_s(ctx->wHost, 256, wHost);
            ctx->is_running = TRUE;
            InitializeCriticalSection(&ctx->lock);
            WinHttpSetStatusCallback(hSession, WInternalNetworkCallback, WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, (DWORD_PTR)ctx);
            WinHttpSetOption(hConnect, WINHTTP_OPTION_CONTEXT_VALUE, &ctx, sizeof(WInternalConnectionContext *));
            ctx->hHeartbeatThread = CreateThread(NULL, 0, WInternalHeartbeatThread, (LPVOID)ctx, 0, NULL);
        }
    }
    return hConnect;
}

BOOL WDisconnect(HINTERNET *phHandle)
{
    BOOL result = FALSE;
    if (phHandle && *phHandle)
    {
        UnregisterHandleFromAutoCleanup(*phHandle);
       
        HINTERNET hSessionParent = NULL;
        DWORD dwLen = sizeof(HINTERNET);

        BOOL bQuerySuccess = WinHttpQueryOption(*phHandle, WINHTTP_OPTION_PARENT_HANDLE, &hSessionParent, &dwLen);
        HandleToStrLog(&hSessionParent, bQuerySuccess, "WDisconnect - Querying Parent hSession");

        HINTERNET hConnectToClose = *phHandle;
        // ====================================================================
        WInternalConnectionContext *ctx = NULL;
        DWORD dwContextLen = sizeof(WInternalConnectionContext *);
        if (WinHttpQueryOption(hConnectToClose, WINHTTP_OPTION_CONTEXT_VALUE, &ctx, &dwContextLen) && ctx)
        {
            ctx->is_running = FALSE;
            if (ctx->hHeartbeatThread)
            {
                WaitForSingleObject(ctx->hHeartbeatThread, 2000);
                BOOL bCloseThread = CloseHandle(ctx->hHeartbeatThread);
                HandleToStrLog(&ctx->hHeartbeatThread, bCloseThread, "WDisconnect - HeartbeatThread Free");
            }
            DeleteCriticalSection(&ctx->lock);
            free(ctx);
            SetLastError(ERROR_SUCCESS);
            HandleToStrLog(&hConnectToClose, TRUE, "WDisconnect - OnChange - Internal Context Memory Freed");
        }
        // ====================================================================

        result = WinHttpCloseHandle(*phHandle);
        HandleToStrLog(&hConnectToClose, result, "WDisconnect - Close hConnect");

        *phHandle = NULL;

        if (hSessionParent)
        {
            BOOL bSessionResult = WinHttpCloseHandle(hSessionParent);
            HandleToStrLog(&hSessionParent, bSessionResult, "WDisconnect - Close hSession");

            // Si el cierre del hConnect fue exitoso pero el del hSession falló,
            // actualizamos el resultado global para que refleje el estatus real.
            if (result)
                result = bSessionResult;
        }
    }
    return result;
}

BOOL WCloseRequest(HINTERNET *phRequest)
{
    if (!phRequest || !*phRequest)
        return FALSE;

    HINTERNET hToClose = *phRequest;
    BOOL result = WinHttpCloseHandle(*phRequest);
    HandleToStrLog(&hToClose, result, "WCloseRequest - Close hRequest");

    *phRequest = NULL;

    return result;
}

BOOL WRequest(
    HINTERNET hConnect,
    const char *szVerb,
    const char *szPath,
    int tlsVersion,
    BOOL isSecure,
    BOOL bIgnoreSslErrors,
    const char *szClientCertSubject,
    const char *szHeaders,
    const char *szContentType,
    const char *szBoundary,
    const char *szTextBody,
    const char *szFilePath,
    char **ppOutResponseBuffer,
    HINTERNET *phOutRequest,
    DWORD *pStatusCode)
{
    if (!hConnect || !ppOutResponseBuffer || !phOutRequest || !pStatusCode)
        return FALSE;

    *pStatusCode = 0;
    *ppOutResponseBuffer = NULL;
    *phOutRequest = NULL;

    wchar_t wVerb[32] = {0};
    wchar_t wPath[2048] = {0};

    MultiByteToWideChar(CP_UTF8, 0, szVerb ? szVerb : "GET", -1, wVerb, 32);
    MultiByteToWideChar(CP_UTF8, 0, szPath ? szPath : "/", -1, wPath, 2048);

    // HeartBeat
    HINTERNET hActiveHandle = hConnect;
    WInternalConnectionContext *ctx = NULL;
    DWORD dwLen = sizeof(WInternalConnectionContext *);
    if (WinHttpQueryOption(hConnect, WINHTTP_OPTION_CONTEXT_VALUE, &ctx, &dwLen) && ctx)
    {
        EnterCriticalSection(&ctx->lock);
        hActiveHandle = ctx->hConnect;
        LeaveCriticalSection(&ctx->lock);
    }
    // HeartBeat
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        wVerb,
        wPath,
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        isSecure ? WINHTTP_FLAG_SECURE : 0);

    HandleToStrLog(&hRequest, (hRequest != NULL), "WRequest - Open hRequest");

    if (!hRequest)
        return FALSE;

    if (isSecure)
    {
        if (tlsVersion > 0)
        {
            DWORD dwProtocols = (DWORD)TlsProtocol(tlsVersion);
            WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURE_PROTOCOLS, &dwProtocols, sizeof(dwProtocols));
        }
        WConfigureSslSecurity(hRequest, bIgnoreSslErrors, szClientCertSubject);
    }

    if (szContentType && szContentType[0] != '\0')
    {
        char szBufCT[2048];
        if (szBoundary && szBoundary[0] != '\0')
        {
            _snprintf_s(szBufCT, sizeof(szBufCT), _TRUNCATE, "Content-Type: %s; boundary=%s\r\n", szContentType, szBoundary);
        }
        else
        {
            _snprintf_s(szBufCT, sizeof(szBufCT), _TRUNCATE, "Content-Type: %s\r\n", szContentType);
        }
        int wLen = MultiByteToWideChar(CP_UTF8, 0, szBufCT, -1, NULL, 0);
        wchar_t *wBufCT = (wchar_t *)malloc(wLen * sizeof(wchar_t));
        if (wBufCT)
        {
            MultiByteToWideChar(CP_UTF8, 0, szBufCT, -1, wBufCT, wLen);
            WinHttpAddRequestHeaders(hRequest, wBufCT, (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
            free(wBufCT);
        }
    }
    // Pongo primero el content type, que se setea por defecto
    // porque si envia en el header lo piso a drede
    if (szHeaders && szHeaders[0] != '\0')
    {
        int wLen = MultiByteToWideChar(CP_UTF8, 0, szHeaders, -1, NULL, 0);
        wchar_t *wHeaders = (wchar_t *)malloc(wLen * sizeof(wchar_t));
        if (wHeaders)
        {
            MultiByteToWideChar(CP_UTF8, 0, szHeaders, -1, wHeaders, wLen);
            WinHttpAddRequestHeaders(hRequest, wHeaders, (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
            free(wHeaders);
        }
    }

    BOOL bSend = FALSE;
    /*
    if (szFilePath && szFilePath[0] != '\0')
    {
        HANDLE hFile = CreateFileA(szFilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            DWORD fileSize = GetFileSize(hFile, NULL);
            BOOL isMultipart = (szBoundary && szBoundary[0] != '\0');
            char szStartBoundary[2048] = {0};
            char szEndBoundary[512] = {0};
            int startLen = 0, endLen = 0;

            if (isMultipart)
            {
                startLen = _snprintf_s(szStartBoundary, sizeof(szStartBoundary), _TRUNCATE,
                                       "--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"archivo\"\r\nContent-Type: application/octet-stream\r\n\r\n", szBoundary);
                endLen = _snprintf_s(szEndBoundary, sizeof(szEndBoundary), _TRUNCATE, "\r\n--%s--\r\n", szBoundary);
            }

            DWORD totalPayloadSize = (DWORD)(startLen + fileSize + endLen);
            bSend = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, totalPayloadSize, 0);

            if (bSend)
            {
                DWORD written = 0;
                if (isMultipart && startLen > 0)
                    WinHttpWriteData(hRequest, szStartBoundary, (DWORD)startLen, &written);

                BYTE chunk[8192];
                DWORD read = 0;
                while (bSend && ReadFile(hFile, chunk, sizeof(chunk), &read, NULL) && read > 0)
                {
                    bSend = WinHttpWriteData(hRequest, chunk, read, &written);
                }

                if (bSend && isMultipart && endLen > 0)
                    WinHttpWriteData(hRequest, szEndBoundary, (DWORD)endLen, &written);
            }
            CloseHandle(hFile);
        }
    }
    else
    {
        DWORD bodyLenBytes = (szTextBody && szTextBody[0] != '\0') ? (DWORD)strlen(szTextBody) : 0;
        bSend = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)szTextBody, bodyLenBytes, bodyLenBytes, 0);
    }*/
    if (szFilePath && szFilePath[0] != '\0')
    {
        HANDLE hFile = CreateFileA(szFilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            DWORD fileSize = GetFileSize(hFile, NULL);
            BOOL isMultipart = (szBoundary && szBoundary[0] != '\0');

            char szStartBoundary[2048] = {0};
            char szEndBoundary[512] = {0};
            int startLen = 0, endLen = 0;

            if (isMultipart)
            {
                const char *szFileName = strrchr(szFilePath, '\\');
                if (szFileName)
                {
                    szFileName++;
                }
                else
                {
                    szFileName = strrchr(szFilePath, '/');
                    if (szFileName)
                    {
                        szFileName++;
                    }
                    else
                    {
                        szFileName = szFilePath;
                    }
                }
                char szInnerCT[128];

                if (strstr(szFileName, ".pdf") || strstr(szFileName, ".PDF"))
                {
                    strcpy_s(szInnerCT, sizeof(szInnerCT), "application/pdf");
                }
                else if (strstr(szFileName, ".jpg") || strstr(szFileName, ".jpeg") ||
                         strstr(szFileName, ".JPG") || strstr(szFileName, ".JPEG"))
                {
                    strcpy_s(szInnerCT, sizeof(szInnerCT), "image/jpeg");
                }
                else if (strstr(szFileName, ".png") || strstr(szFileName, ".PNG"))
                {
                    strcpy_s(szInnerCT, sizeof(szInnerCT), "image/png");
                }
                else if (strstr(szFileName, ".xml") || strstr(szFileName, ".XML"))
                {
                    strcpy_s(szInnerCT, sizeof(szInnerCT), "application/xml");
                }
                else if (strstr(szFileName, ".zip") || strstr(szFileName, ".ZIP"))
                {
                    strcpy_s(szInnerCT, sizeof(szInnerCT), "application/zip");
                }
                else
                {
                    GetMimeTypeFromExtension(szFileName, szInnerCT, sizeof(szInnerCT));
                }
                startLen = _snprintf_s(szStartBoundary, sizeof(szStartBoundary), _TRUNCATE,
                                       "--%s\r\n"
                                       "Content-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n"
                                       "Content-Type: %s\r\n\r\n",
                                       szBoundary, szFileName, szInnerCT);
                endLen = _snprintf_s(szEndBoundary, sizeof(szEndBoundary), _TRUNCATE, "\r\n--%s--\r\n", szBoundary);
            }

            DWORD totalPayloadSize = (DWORD)(startLen + fileSize + endLen);
            bSend = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, totalPayloadSize, 0);

            if (bSend)
            {
                DWORD written = 0;
                if (isMultipart && startLen > 0)
                    WinHttpWriteData(hRequest, szStartBoundary, (DWORD)startLen, &written);

                BYTE chunk[8192];
                DWORD read = 0;
                while (bSend && ReadFile(hFile, chunk, sizeof(chunk), &read, NULL) && read > 0)
                {
                    bSend = WinHttpWriteData(hRequest, chunk, read, &written);
                }
                if (bSend && isMultipart && endLen > 0)
                    WinHttpWriteData(hRequest, szEndBoundary, (DWORD)endLen, &written);
            }
            CloseHandle(hFile);
        }
    }
    else
    {
        DWORD bodyLenBytes = (szTextBody && szTextBody[0] != '\0') ? (DWORD)strlen(szTextBody) : 0;
        bSend = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)szTextBody, bodyLenBytes, bodyLenBytes, 0);
    }

    if (!bSend)
    {
        WCloseRequest(&hRequest);
        return FALSE;
    }

    bSend = WinHttpReceiveResponse(hRequest, NULL);

    // if( !bSend && GetLastError( ) == ERROR_WINHTTP_RESEND_REQUEST)
    // continue;
    // https://learn.microsoft.com/en-us/windows/win32/winhttp/authentication-in-winhttp

    if (!bSend)
    {
        WCloseRequest(&hRequest);
        return FALSE;
    }

    DWORD dwStatusCode = 0;
    DWORD dwSize = sizeof(dwStatusCode);
    if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX))
    {
        *pStatusCode = dwStatusCode;
    }

    char *responseBuffer = NULL;
    DWORD totalBytesRead = 0;
    DWORD dwChunkSize = 0;
    DWORD dwRead = 0;
    BOOL bReadSuccess = TRUE;

    do
    {
        dwChunkSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwChunkSize))
        {
            bReadSuccess = FALSE;
            break;
        }
        if (dwChunkSize == 0)
            break;

        char *temp = (char *)realloc(responseBuffer, totalBytesRead + dwChunkSize + 1);
        if (!temp)
        {
            bReadSuccess = FALSE;
            break;
        }
        responseBuffer = temp;

        if (!WinHttpReadData(hRequest, (LPVOID)(responseBuffer + totalBytesRead), dwChunkSize, &dwRead))
        {
            bReadSuccess = FALSE;
            break;
        }
        totalBytesRead += dwRead;
    } while (dwChunkSize > 0);

    if (!bReadSuccess)
    {
        if (responseBuffer)
            free(responseBuffer);
        WCloseRequest(&hRequest);
        return FALSE;
    }

    if (responseBuffer && totalBytesRead > 0)
    {
        responseBuffer[totalBytesRead] = '\0';
        // int OutLen = 0;
        // char *utf8Buffer = StrAutoToUtf8(responseBuffer, (int)strlen(responseBuffer), &OutLen);
        // if (utf8Buffer && OutLen > 0)
        //  *ppOutResponseBuffer =  utf8Buffer;
        *ppOutResponseBuffer = responseBuffer;
        // *ppOutResponseBuffer = StrAutoToUtf8 responseBuffer;
    }

    *phOutRequest = hRequest;
    return TRUE;
}