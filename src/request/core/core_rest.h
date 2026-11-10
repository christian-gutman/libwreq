#ifndef CORE_REST_H
#define CORE_REST_H

#include "core_request.h"
 

#ifdef __cplusplus
extern "C" {
#endif


static __forceinline char *BuildMLHeaders(const char *szAccessToken, const char *szExtraHeader)
{
    char *szFinalHeaders = NULL;

    if (szAccessToken && szAccessToken[0] != '\0')
    {
        const char *authFormat = "Authorization: Bearer %s\r\n%s";
        const char *extra = szExtraHeader ? szExtraHeader : "";

        int reqHeaderSize = _scprintf(authFormat, szAccessToken, extra);
        if (reqHeaderSize > 0)
        {
            szFinalHeaders = (char *)malloc(reqHeaderSize + 1);
            if (szFinalHeaders)
            {
                sprintf_s(szFinalHeaders, reqHeaderSize + 1, authFormat, szAccessToken, extra);
            }
        }
    }
    else if (szExtraHeader && szExtraHeader[0] != '\0')
    {
        szFinalHeaders = _strdup(szExtraHeader);
    }

    return szFinalHeaders;
}

CORE_API BOOL WRestRequest(
    HINTERNET hConnect, const char *szVerb, const char *szPath, const char *szHeaders,
    const char *szContentType, const char *szBoundary, const char *szTextBody, const char *szFilePath, // <- AGREGADO szFilePath
    char **ppOutResponseBuffer, DWORD *pStatusCode);

CORE_API BOOL WRestPut(
    HINTERNET hConnect, const char *szPath, const char *szHeaders, const char *szTextBody, 
    char **ppOutResponseBuffer, DWORD *pStatusCode);

CORE_API BOOL WRestPost(
    HINTERNET hConnect, const char *szPath, const char *szHeaders, const char *szTextBody, 
    char **ppOutResponseBuffer, DWORD *pStatusCode);

CORE_API BOOL WRestPatch(
    HINTERNET hConnect, const char *szPath, const char *szHeaders, const char *szTextBody, 
    char **ppOutResponseBuffer, DWORD *pStatusCode);

CORE_API BOOL WRestDelete(
    HINTERNET hConnect, const char *szPath, const char *szHeaders, 
    char **ppOutResponseBuffer, DWORD *pStatusCode);

CORE_API BOOL WRestGet(
    HINTERNET hConnect, const char *szPath, const char *szHeaders, 
    char **ppOutResponseBuffer, DWORD *pStatusCode);

CORE_API BOOL WRestOptions(
    HINTERNET hConnect, const char *szPath, const char *szHeaders, const char *szParams, 
    char **ppOutResponseBuffer, DWORD *pStatusCode);

CORE_API BOOL WRestFormPost(
    HINTERNET hConnect, const char *szPath, const char *szToken, const char *szTextBody, 
    char **ppOutResponseBuffer, DWORD *pStatusCode);

CORE_API BOOL WRestFormPut(
    HINTERNET hConnect, const char *szPath, const char *szToken, const char *szTextBody, 
    char **ppOutResponseBuffer, DWORD *pStatusCode);

CORE_API BOOL WRestFormPatch(
    HINTERNET hConnect, const char *szPath, const char *szToken, const char *szTextBody, 
    char **ppOutResponseBuffer, DWORD *pStatusCode);

CORE_API BOOL WRestMultipartPost(
    HINTERNET hConnect, const char *szPath, const char *szHeaders, const char *szBoundary,
    const char *szFilePath, char **ppOutResponseBuffer, DWORD *pStatusCode);

#ifdef __cplusplus
}
#endif

#endif // CORE_REST_H
