#include "core_rest.h"

BOOL WRestRequest(
    HINTERNET hConnect, const char *szVerb, const char *szPath, const char *szHeaders,
    const char *szContentType, const char *szBoundary, const char *szTextBody, const char *szFilePath, // <- AGREGADO szFilePath
    char **ppOutResponseBuffer, DWORD *pStatusCode)
{
    HINTERNET hLocalRequest = NULL;
    if (ppOutResponseBuffer)
    {
        *ppOutResponseBuffer = NULL;
    }

    const char *actualHeaders = (szHeaders && szHeaders[0] != '\0') ? szHeaders : "Accept: application/json\r\n";
    const char *actualContentType = (szContentType && szContentType[0] != '\0') ? szContentType : "application/json";
    const char *actualTextBody = (szTextBody && szTextBody[0] != '\0') ? szTextBody : NULL;
    const char *actualFilePath = (szFilePath && szFilePath[0] != '\0') ? szFilePath : NULL; // <- Nueva validación

    // CORRECCIÓN: Ahora pasamos actualFilePath en la posición número 12, justo después de actualTextBody
    BOOL bResult = WRequest(
        hConnect, szVerb, szPath, 5, // TLS 1.2/1.3
        TRUE, FALSE, NULL,
        actualHeaders, actualContentType, szBoundary,
        actualTextBody, actualFilePath, ppOutResponseBuffer, &hLocalRequest, pStatusCode);

    WCloseRequest(&hLocalRequest);

    if (!bResult && pStatusCode && *pStatusCode == 0)
    {
        *pStatusCode = GetLastError();
    }
    return bResult;
}

BOOL WRestMultipartPost(
    HINTERNET hConnect, const char *szPath, const char *szHeaders, const char *szBoundary,
    const char *szFilePath, char **ppOutResponseBuffer, DWORD *pStatusCode)
{
    return WRestRequest(
        hConnect, "POST", szPath, szHeaders,
        "multipart/form-data",
        szBoundary,
        NULL,       // szTextBody va en NULL porque no mandamos texto json
        szFilePath, // szFilePath va aquí para que WRestRequest lo reciba y lo mande a CreateFileA
        ppOutResponseBuffer,
        pStatusCode);
}

BOOL WRestGet(HINTERNET hConnect, const char *szPath, const char *szHeaders, char **ppOutResponseBuffer, DWORD *pStatusCode)
{
    return WRestRequest(hConnect, "GET", szPath, szHeaders, "application/json", NULL, NULL, NULL, ppOutResponseBuffer, pStatusCode);
}
BOOL WRestPost(HINTERNET hConnect, const char *szPath, const char *szHeaders, const char *szTextBody, char **ppOutResponseBuffer, DWORD *pStatusCode)
{
    return WRestRequest(hConnect, "POST", szPath, szHeaders, "application/json", NULL, szTextBody, NULL, ppOutResponseBuffer, pStatusCode);
}
BOOL WRestPut(HINTERNET hConnect, const char *szPath, const char *szHeaders, const char *szTextBody, char **ppOutResponseBuffer, DWORD *pStatusCode)
{
    return WRestRequest(hConnect, "PUT", szPath, szHeaders, "application/json", NULL, szTextBody, NULL, ppOutResponseBuffer, pStatusCode);
}
BOOL WRestPatch(HINTERNET hConnect, const char *szPath, const char *szHeaders, const char *szTextBody, char **ppOutResponseBuffer, DWORD *pStatusCode)
{
    return WRestRequest(hConnect, "PATCH", szPath, szHeaders, "application/json", NULL, szTextBody, NULL, ppOutResponseBuffer, pStatusCode);
}
BOOL WRestDelete(HINTERNET hConnect, const char *szPath, const char *szHeaders, char **ppOutResponseBuffer, DWORD *pStatusCode)
{
    return WRestRequest(hConnect, "DELETE", szPath, szHeaders, "application/json", NULL, NULL, NULL, ppOutResponseBuffer, pStatusCode);
}
BOOL WRestOptions(HINTERNET hConnect, const char *szPath, const char *szHeaders, const char *szParams, char **ppOutResponseBuffer, DWORD *pStatusCode)
{
    return WRestRequest(hConnect, "OPTIONS", szPath, szHeaders, "application/json", NULL, szParams, NULL, ppOutResponseBuffer, pStatusCode);
}
BOOL WRestFormPost(
    HINTERNET hConnect, const char *szPath, const char *szToken, const char *szTextBody,
    char **ppOutResponseBuffer, DWORD *pStatusCode)
{
    char szHeaders[1024] = "Accept: application/json\r\nContent-type: application/json\r\n";
    if (szToken && szToken[0] != '\0')
    {
        char szAuthHeader[512];
        _snprintf_s(szAuthHeader, sizeof(szAuthHeader), _TRUNCATE, "Authorization: Bearer %s\r\n", szToken);
        strcat_s(szHeaders, sizeof(szHeaders), szAuthHeader);
    }
    return WRestRequest(
        hConnect, "POST", szPath, szHeaders,
        "application/x-www-form-urlencoded",
        NULL,       // szBoundary (No aplica a formularios web)
        szTextBody, // Cuerpo de texto del formulario codificado
        NULL,       // szFilePath (No aplica, se pasa NULL)
        ppOutResponseBuffer,
        pStatusCode);
}

BOOL WRestFormPut(
    HINTERNET hConnect, const char *szPath, const char *szToken, const char *szTextBody,
    char **ppOutResponseBuffer, DWORD *pStatusCode)
{
    char szHeaders[1024] = "Accept: application/json\r\n";
    if (szToken && szToken[0] != '\0')
    {
        char szAuthHeader[512];
        _snprintf_s(szAuthHeader, sizeof(szAuthHeader), _TRUNCATE, "Authorization: Bearer %s\r\n", szToken);
        strcat_s(szHeaders, sizeof(szHeaders), szAuthHeader);
    }

    return WRestRequest(
        hConnect, "PUT", szPath, szHeaders,
        "application/x-www-form-urlencoded",
        NULL,       // szBoundary
        szTextBody, // szTextBody
        NULL,       // szFilePath
        ppOutResponseBuffer,
        pStatusCode);
}

BOOL WRestFormPatch(
    HINTERNET hConnect, const char *szPath, const char *szToken, const char *szTextBody,
    char **ppOutResponseBuffer, DWORD *pStatusCode)
{
    char szHeaders[1024] = "Accept: application/json\r\n";
    if (szToken && szToken[0] != '\0')
    {
        char szAuthHeader[512];
        _snprintf_s(szAuthHeader, sizeof(szAuthHeader), _TRUNCATE, "Authorization: Bearer %s\r\n", szToken);
        strcat_s(szHeaders, sizeof(szHeaders), szAuthHeader);
    }
    return WRestRequest(
        hConnect, "PATCH", szPath, szHeaders,
        "application/x-www-form-urlencoded",
        NULL,       // szBoundary
        szTextBody, // szTextBody
        NULL,       // szFilePath
        ppOutResponseBuffer,
        pStatusCode);
}