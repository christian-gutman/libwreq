#include "core_req_file.h"

BOOL WDownloadToFile(
    HINTERNET hConnect, const char *szVerb, const char *szPath, int tlsVersion,
    BOOL isSecure, BOOL bIgnoreSslErrors, const char *szClientCertSubject,
    const char *szHeaders, const char *szContentType, const char *szSaveFilePath,
    DWORD *pStatusCode)
{
    if (!hConnect || !szSaveFilePath || szSaveFilePath[0] == '\0' || !pStatusCode)
        return FALSE;
    *pStatusCode = 0;

    wchar_t *wVerb = (szVerb && szVerb[0] != '\0') ? StrAutoToUnicode(szVerb, -1, NULL) : NULL;
    wchar_t *wPath = (szPath && szPath[0] != '\0') ? StrAutoToUnicode(szPath, -1, NULL) : NULL;

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, wVerb ? wVerb : L"GET", wPath ? wPath : L"/",
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, isSecure ? WINHTTP_FLAG_SECURE : 0);

    if (wVerb)
        free(wVerb);
    if (wPath)
        free(wPath);
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

    if (szHeaders && szHeaders[0] != '\0')
    {
        wchar_t *wHeaders = StrAutoToUnicode(szHeaders, -1, NULL);
        if (wHeaders)
        {
            WinHttpAddRequestHeaders(hRequest, wHeaders, (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
            free(wHeaders);
        }
    }

    if (szContentType && szContentType[0] != '\0')
    {
        char szBufCT[2048];
        _snprintf_s(szBufCT, sizeof(szBufCT), _TRUNCATE, "Content-Type: %s", szContentType);
        wchar_t *wBufCT = StrAutoToUnicode(szBufCT, -1, NULL);
        if (wBufCT)
        {
            WinHttpAddRequestHeaders(hRequest, wBufCT, (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
            free(wBufCT);
        }
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hRequest, NULL))
    {
        WinHttpCloseHandle(hRequest);
        return FALSE;
    }

    DWORD dwSize = sizeof(*pStatusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, pStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);

    // CORRECCIÓN PROTECCIÓN: Si el servidor responde un error (ej: 400, 404, 500), NO creamos el archivo en disco
    if (*pStatusCode != 200 && *pStatusCode != 201)
    {
        WinHttpCloseHandle(hRequest);
        return FALSE;
    }

    BOOL bDownloadSuccess = FALSE;
    wchar_t *wSaveFilePath = StrAutoToUnicode(szSaveFilePath, -1, NULL);
    if (!wSaveFilePath)
    {
        WinHttpCloseHandle(hRequest);
        return FALSE;
    }

    HANDLE hFile = CreateFileW(wSaveFilePath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        BYTE chunkBuffer[8192];
        DWORD dwSizeChunk = 0, dwRead = 0, dwWritten = 0;
        bDownloadSuccess = TRUE;

        do
        {
            dwSizeChunk = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSizeChunk))
            {
                bDownloadSuccess = FALSE;
                break;
            }
            if (dwSizeChunk == 0)
                break;

            DWORD bytesToRead = (dwSizeChunk > sizeof(chunkBuffer)) ? sizeof(chunkBuffer) : dwSizeChunk;
            if (WinHttpReadData(hRequest, (LPVOID)chunkBuffer, bytesToRead, &dwRead) && dwRead > 0)
            {
                if (!WriteFile(hFile, (LPCVOID)chunkBuffer, dwRead, &dwWritten, NULL) || dwWritten != dwRead)
                {
                    bDownloadSuccess = FALSE;
                    break;
                }
            }
            else
            {
                bDownloadSuccess = FALSE;
                break;
            }
        } while (dwSizeChunk > 0);

        CloseHandle(hFile);
        if (!bDownloadSuccess)
        {
            DeleteFileW(wSaveFilePath);
        }
    }

    free(wSaveFilePath);
    WinHttpCloseHandle(hRequest);
    return bDownloadSuccess;
}

BOOL WRequestToFile(
    HINTERNET hConnect, const char *szVerb, const char *szPath, int tlsVersion,
    BOOL isSecure, BOOL bIgnoreSslErrors, const char *szClientCertSubject,
    const char *szHeaders, const char *szContentType, const char *szBoundary,
    const char *szInputFilePath, const char *szOutputFilePath, DWORD *pStatusCode)
{
    if (!hConnect || !szInputFilePath || szInputFilePath[0] == '\0' || !szOutputFilePath || szOutputFilePath[0] == '\0' || !pStatusCode)
        return FALSE;
    *pStatusCode = 0;

    wchar_t *wVerb = (szVerb && szVerb[0] != '\0') ? StrAutoToUnicode(szVerb, -1, NULL) : NULL;
    wchar_t *wPath = (szPath && szPath[0] != '\0') ? StrAutoToUnicode(szPath, -1, NULL) : NULL;

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, wVerb ? wVerb : L"POST", wPath ? wPath : L"/",
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, isSecure ? WINHTTP_FLAG_SECURE : 0);

    if (wVerb)
        free(wVerb);
    if (wPath)
        free(wPath);
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

    if (szHeaders && szHeaders[0] != '\0')
    {
        wchar_t *wHeaders = StrAutoToUnicode(szHeaders, -1, NULL);
        if (wHeaders)
        {
            WinHttpAddRequestHeaders(hRequest, wHeaders, (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
            free(wHeaders);
        }
    }

    if (szContentType && szContentType[0] != '\0')
    {
        char szBufCT[2048];
        if (szBoundary && szBoundary[0] != '\0')
        {
            _snprintf_s(szBufCT, sizeof(szBufCT), _TRUNCATE, "Content-Type: %s; boundary=%s", szContentType, szBoundary);
        }
        else
        {
            _snprintf_s(szBufCT, sizeof(szBufCT), _TRUNCATE, "Content-Type: %s", szContentType);
        }
        wchar_t *wBufCT = StrAutoToUnicode(szBufCT, -1, NULL);
        if (wBufCT)
        {
            WinHttpAddRequestHeaders(hRequest, wBufCT, (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
            free(wBufCT);
        }
    }

    BOOL bSend = FALSE;
    wchar_t *wInputFilePath = StrAutoToUnicode(szInputFilePath, -1, NULL);
    if (wInputFilePath)
    {
        HANDLE hFileInput = CreateFileW(wInputFilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFileInput != INVALID_HANDLE_VALUE)
        {
            DWORD fileSize = GetFileSize(hFileInput, NULL);
            BOOL isMultipart = (szBoundary && szBoundary[0] != '\0');
            char szStartBoundary[2048];
            char szEndBoundary[512];
            int startLen = 0, endLen = 0;

            if (isMultipart)
            {
                // CORRECCIÓN CRÍTICA: Nombre y tipo MIME dinámicos para Mercado Libre
                const char *szFileName = strrchr(szInputFilePath, '\\');
                if (szFileName)
                    szFileName++;
                else
                    szFileName = szInputFilePath;

                char szInnerCT[128];
                GetMimeTypeFromExtension(szFileName, szInnerCT, sizeof(szInnerCT));

                startLen = _snprintf_s(szStartBoundary, sizeof(szStartBoundary), _TRUNCATE,
                                       "--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\nContent-Type: %s\r\n\r\n",
                                       szBoundary, szFileName, szInnerCT);
                endLen = _snprintf_s(szEndBoundary, sizeof(szEndBoundary), _TRUNCATE, "\r\n--%s--\r\n", szBoundary);
            }

            DWORD totalPayloadSize = (DWORD)(startLen + fileSize + endLen);
            bSend = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, totalPayloadSize, 0);

            if (bSend)
            {
                DWORD written = 0;
                if (isMultipart && startLen > 0)
                {
                    WinHttpWriteData(hRequest, szStartBoundary, (DWORD)startLen, &written);
                }
                BYTE chunk[8192];
                DWORD read = 0;
                while (bSend && ReadFile(hFileInput, chunk, sizeof(chunk), &read, NULL) && read > 0)
                {
                    bSend = WinHttpWriteData(hRequest, chunk, read, &written);
                }
                if (bSend && isMultipart && endLen > 0)
                {
                    WinHttpWriteData(hRequest, szEndBoundary, (DWORD)endLen, &written);
                }
            }
            CloseHandle(hFileInput);
        }
        free(wInputFilePath);
    }

    if (!bSend || !WinHttpReceiveResponse(hRequest, NULL))
    {
        WinHttpCloseHandle(hRequest);
        return FALSE;
    }

    DWORD dwSize = sizeof(*pStatusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, pStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);

    if (*pStatusCode != 200 && *pStatusCode != 201)
    {
        WinHttpCloseHandle(hRequest);
        return FALSE;
    }

    BOOL bDownloadSuccess = FALSE;
    wchar_t *wOutputFilePath = StrAutoToUnicode(szOutputFilePath, -1, NULL);
    if (wOutputFilePath)
    {
        HANDLE hFileOutput = CreateFileW(wOutputFilePath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFileOutput != INVALID_HANDLE_VALUE)
        {
            BYTE chunkBuffer[8192];
            DWORD dwSizeChunk = 0, dwRead = 0, dwWritten = 0;
            bDownloadSuccess = TRUE;

            do
            {
                dwSizeChunk = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &dwSizeChunk))
                {
                    bDownloadSuccess = FALSE;
                    break;
                }
                if (dwSizeChunk == 0)
                    break;
                DWORD bytesToRead = (dwSizeChunk > sizeof(chunkBuffer)) ? sizeof(chunkBuffer) : dwSizeChunk;
                if (WinHttpReadData(hRequest, (LPVOID)chunkBuffer, bytesToRead, &dwRead) && dwRead > 0)
                {
                    if (!WriteFile(hFileOutput, (LPCVOID)chunkBuffer, dwRead, &dwWritten, NULL) || dwWritten != dwRead)
                    {
                        bDownloadSuccess = FALSE;
                        break;
                    }
                }
                else
                {
                    bDownloadSuccess = FALSE;
                    break;
                }
            } while (dwSizeChunk > 0);
            CloseHandle(hFileOutput);
            if (!bDownloadSuccess)
            {
                DeleteFileW(wOutputFilePath);
            }
        }
        free(wOutputFilePath);
    }
    WinHttpCloseHandle(hRequest);
    return bDownloadSuccess;
}

// Definición del prototipo para el callback de progreso compatible
typedef void (*FnProgressCallback)(int nTarea, ULONGLONG nProgreso, ULONGLONG nTotal, void *pParam);

BOOL WRequestToStream(
    HINTERNET hConnect, const char *szVerb, const char *szPath, int tlsVersion,
    BOOL isSecure, BOOL bIgnoreSslErrors, const char *szClientCertSubject,
    const char *szHeaders, const char *szContentType,
    const char *szPostData, DWORD dwPostDataLen,             // Payload desde memoria
    const wchar_t *wSaveFilePath,                             // Destino en disco (Opcional)
    char **ppOutErrorBuffer, DWORD *pdwOutErrorLen,           // Captura de errores (XML/Texto)
    DWORD *pStatusCode,
    FnProgressCallback fnCallback, void *pParamUsuario)
{
    if (!hConnect || !pStatusCode) return FALSE;
    *pStatusCode = 0;
    if (ppOutErrorBuffer) *ppOutErrorBuffer = NULL;
    if (pdwOutErrorLen) *pdwOutErrorLen = 0;

    wchar_t *wVerb = (szVerb && szVerb[0] != '\0') ? StrAutoToUnicode(szVerb, -1, NULL) : NULL;
    wchar_t *wPath = (szPath && szPath[0] != '\0') ? StrAutoToUnicode(szPath, -1, NULL) : NULL;

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, wVerb ? wVerb : L"GET", wPath ? wPath : L"/",
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, isSecure ? WINHTTP_FLAG_SECURE : 0);

    if (wVerb) free(wVerb);
    if (wPath) free(wPath);
    if (!hRequest) return FALSE;

    if (isSecure) {
        if (tlsVersion > 0) {
            DWORD dwProtocols = (DWORD)TlsProtocol(tlsVersion);
            WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURE_PROTOCOLS, &dwProtocols, sizeof(dwProtocols));
        }
        WConfigureSslSecurity(hRequest, bIgnoreSslErrors, szClientCertSubject);
    }

    if (szHeaders && szHeaders[0] != '\0') {
        wchar_t *wHeaders = StrAutoToUnicode(szHeaders, -1, NULL);
        if (wHeaders) {
            WinHttpAddRequestHeaders(hRequest, wHeaders, (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
            free(wHeaders);
        }
    }

    if (szContentType && szContentType[0] != '\0') {
        wchar_t *wBufCT = StrAutoToUnicode(szContentType, -1, NULL);
        if (wBufCT) {
            WinHttpAddRequestHeaders(hRequest, wBufCT, (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
            free(wBufCT);
        }
    }

    // Envío de datos (POST/PUT) desde memoria si existen
    BOOL bSend = FALSE;
    if (szPostData && dwPostDataLen > 0) {
        bSend = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)szPostData, dwPostDataLen, dwPostDataLen, 0);
    } else {
        bSend = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    }

    if (!bSend || !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        return FALSE;
    }

    DWORD dwSize = sizeof(*pStatusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, pStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);

    // Verificar el tipo de contenido retornado
    wchar_t contentType[128] = {0};
    dwSize = sizeof(contentType);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_TYPE, WINHTTP_HEADER_NAME_BY_INDEX, contentType, &dwSize, WINHTTP_NO_HEADER_INDEX);

    BOOL bIsXmlOrText = (wcsstr(contentType, L"text/xml") || wcsstr(contentType, L"text/plain") || wcsstr(contentType, L"application/xml"));
    BOOL bDownloadSuccess = FALSE;

    if (*pStatusCode == 200 && wSaveFilePath && !bIsXmlOrText) {
        // --- FLUJO A: Descarga binaria directa a Archivo (ZIP) ---
        ULONGLONG totalBytes = 0;
        wchar_t contentLengthHeader[64] = {0};
        DWORD dwCLSize = sizeof(contentLengthHeader);
        if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, contentLengthHeader, &dwCLSize, WINHTTP_NO_HEADER_INDEX)) {
            totalBytes = _wtoi64(contentLengthHeader);
        }

        HANDLE hFile = CreateFileW(wSaveFilePath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            BYTE chunkBuffer[8192];
            DWORD dwSizeChunk = 0, dwRead = 0, dwWritten = 0;
            ULONGLONG acumuladoBytes = 0;
            bDownloadSuccess = TRUE;

            do {
                dwSizeChunk = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &dwSizeChunk)) {
                    bDownloadSuccess = FALSE;
                    break;
                }
                if (dwSizeChunk == 0) break;

                DWORD bytesToRead = (dwSizeChunk > sizeof(chunkBuffer)) ? sizeof(chunkBuffer) : dwSizeChunk;
                if (WinHttpReadData(hRequest, (LPVOID)chunkBuffer, bytesToRead, &dwRead) && dwRead > 0) {
                    if (!WriteFile(hFile, (LPCVOID)chunkBuffer, dwRead, &dwWritten, NULL) || dwWritten != dwRead) {
                        bDownloadSuccess = FALSE;
                        break;
                    }
                    acumuladoBytes += dwRead;
                    if (fnCallback) {
                        fnCallback(1, acumuladoBytes, totalBytes, pParamUsuario);
                    }
                } else {
                    bDownloadSuccess = FALSE;
                    break;
                }
            } while (dwSizeChunk > 0);

            CloseHandle(hFile);
            if (!bDownloadSuccess) {
                DeleteFileW(wSaveFilePath);
            }
        }
    } else {
        // --- FLUJO B: Volcado a Memoria (Errores XML o respuestas de control) ---
        char *errorBuffer = NULL;
        DWORD totalBytesRead = 0;
        DWORD dwSizeChunk = 0, dwRead = 0;
        bDownloadSuccess = TRUE;

        do {
            dwSizeChunk = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSizeChunk)) {
                bDownloadSuccess = FALSE;
                break;
            }
            if (dwSizeChunk == 0) break;

            char *temp = (char *)realloc(errorBuffer, totalBytesRead + dwSizeChunk + 1);
            if (!temp) {
                bDownloadSuccess = FALSE;
                break;
            }
            errorBuffer = temp;

            if (WinHttpReadData(hRequest, (LPVOID)(errorBuffer + totalBytesRead), dwSizeChunk, &dwRead)) {
                totalBytesRead += dwRead;
            } else {
                bDownloadSuccess = FALSE;
                break;
            }
        } while (dwSizeChunk > 0);

        if (bDownloadSuccess && errorBuffer && totalBytesRead > 0) {
            errorBuffer[totalBytesRead] = '\0';
            if (ppOutErrorBuffer && pdwOutErrorLen) {
                *ppOutErrorBuffer = errorBuffer;
                *pdwOutErrorLen = totalBytesRead;
            } else {
                free(errorBuffer);
            }
        } else if (errorBuffer) {
            free(errorBuffer);
        }
    }

    WinHttpCloseHandle(hRequest);
    return bDownloadSuccess;
}
