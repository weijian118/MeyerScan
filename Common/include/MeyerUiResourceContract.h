// Keep this contract header ASCII-only because it is included by both C++ and
// the Visual Studio 2015 resource compiler. The numeric and text macros must
// stay paired so Windows file details show the same values used at runtime.
#ifndef MEYER_UI_RESOURCE_CONTRACT_H
#define MEYER_UI_RESOURCE_CONTRACT_H

#define MEYER_UI_RESOURCE_API_VERSION 1
#define MEYER_UI_RESOURCE_API_VERSION_TEXT "1"

#define MEYER_UI_RESOURCE_PAYLOAD_ID 101
#define MEYER_UI_RESOURCE_PAYLOAD_ID_TEXT "101"

#define MEYER_UI_RESOURCE_MANIFEST_SCHEMA_VERSION 1
#define MEYER_UI_RESOURCE_MANIFEST_SCHEMA_VERSION_TEXT "1"

#define MEYER_UI_RESOURCE_QRC_PREFIX "/MeyerScan/Modules"
#define MEYER_UI_RESOURCE_RUNTIME_ROOT ":/MeyerScan/Modules"
#define MEYER_UI_RESOURCE_MANIFEST_NAME "MeyerScanUiResources.qrc"

#define MEYER_UI_RESOURCE_EXPORTS_TEXT \
    "MeyerScanInitializeUiResources;MeyerScanUiResourcesInitialized;" \
    "MeyerScanShutdownUiResources;GetMeyerUiResourcesApiVersion;" \
    "GetMeyerUiResourcesPayloadId;GetMeyerUiResourcesManifestSchemaVersion;" \
    "GetMeyerUiResourcesPrefix;GetMeyerModuleVersion"

#endif
