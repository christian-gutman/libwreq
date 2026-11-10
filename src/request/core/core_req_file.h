#ifndef CORE_REQ_FILE_H
#define CORE_REQ_FILE_H

#include "core_request.h"

#ifdef __cplusplus
extern "C"
{
#endif
/*
DWORD statusCode = 0;
BOOL ok = WDownloadToFile(
    hConnect, 
    "GET", 
    "/imagenes/foto.jpg", 
    0, TRUE, FALSE, NULL, NULL, 
    "image/jpeg", 
    "C:\\Descargas\\foto.jpg", 
    &statusCode
);
*/
CORE_API BOOL WDownloadToFile(
    HINTERNET hConnect, const char *szVerb, const char *szPath, int tlsVersion,
    BOOL isSecure, BOOL bIgnoreSslErrors, const char *szClientCertSubject,
    const char *szHeaders, const char *szContentType, const char *szSaveFilePath,
    DWORD *pStatusCode);

/*
DWORD statusCode = 0;
BOOL ok = WRequestToFile(
    hConnect, "POST", "/api/subir-comprobante", 0, TRUE, FALSE, NULL, NULL, 
    "multipart/form-data", "----MiBoundary123", 
    "C:\\Documentos\\factura.pdf", "C:\\Destino\\respuesta.json", &statusCode
);
*/    
CORE_API BOOL WRequestToFile(
    HINTERNET hConnect, const char *szVerb, const char *szPath, int tlsVersion,
    BOOL isSecure, BOOL bIgnoreSslErrors, const char *szClientCertSubject,
    const char *szHeaders, const char *szContentType, const char *szBoundary,
    const char *szInputFilePath, const char *szOutputFilePath, DWORD *pStatusCode);

    // Definición del prototipo para el callback de progreso compatible
typedef void (*FnProgressCallback)(int nTarea, ULONGLONG nProgreso, ULONGLONG nTotal, void *pParam);

/*
char *xmlError = NULL;
DWORD xmlErrorLen = 0;
DWORD status = 0;

BOOL descargado = WRequestToStream(
    hConnect, "POST", "/dfeServicioDescargaPadron.do", 0, TRUE, FALSE, NULL, NULL,
    "multipart/form-data; boundary=----WebKitFormBoundaryVL3fdve1Hm4Big3a",
    payloadMemoria, payloadLenBytes, L"C:\\Padrones\\arba_padron.zip",
    &xmlError, &xmlErrorLen, &status, MiProgreso, NULL
);

if (!descargado && xmlError != NULL) {
    printf("Error de ARBA recibido: %s\n", xmlError);
    free(xmlError);
}
*/
CORE_API BOOL WRequestToStream(
    HINTERNET hConnect, const char *szVerb, const char *szPath, int tlsVersion,
    BOOL isSecure, BOOL bIgnoreSslErrors, const char *szClientCertSubject,
    const char *szHeaders, const char *szContentType,
    const char *szPostData, DWORD dwPostDataLen,             // Payload desde memoria
    const wchar_t *wSaveFilePath,                             // Destino en disco (Opcional)
    char **ppOutErrorBuffer, DWORD *pdwOutErrorLen,           // Captura de errores (XML/Texto)
    DWORD *pStatusCode,
    FnProgressCallback fnCallback, void *pParamUser);

#ifdef __cplusplus
}
#endif

#endif // CORE_REQ_FILE_H