#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define BROKER_URI "broker_uri"
#define LOG_LEVEL "log_level"
#define LOG_FILE_PATH "log_file_path"
#define LOG_MAX_SIZE "log_max_size"
#define LOG_BACKUP_NUM "log_backup_num"
#define MB 1024*1024

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <libconfig.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <libgen.h>   //in order to use dirname()
#include "CentralManager.h"
#include "CAPLogger.h"
#include "cap_common.h"

typedef struct _SConfigData{
    char *pszBrokerURI;
	int nLogLevel;
	cap_string strLogFilePath;
	int nLogMaxSize;
	int nLogBackupNum;
} SConfigData;

cap_handle g_hLogger = NULL;

static int getConfigData(SConfigData *pstConfigData, char *pszConfigPath)
{
	cap_result result = ERR_CAP_UNKNOWN;
    const char *pszBrokerURI = NULL;
	int nLogLevel;
	const char *pszLogFilePath = NULL;
	int nLogMaxSize;
	int nLogBackupNum;

	config_t cfg, *cf;
	char *pszLogPath = NULL;
	cf = &cfg;
    config_init(cf);

	char *pszConfigDirPath = NULL;

    if (!config_read_file(cf, pszConfigPath)) {
        fprintf(stderr, "%s:%d - %s %s\n",
            config_error_file(cf),
            config_error_line(cf),
            config_error_text(cf), pszConfigPath);
        config_destroy(cf);
        return(EXIT_FAILURE);
    }

    if (!config_lookup_string(cf, BROKER_URI, &pszBrokerURI)){
        fprintf(stderr, "broker_uri error\n");
    }

	if (!config_lookup_int(cf, LOG_LEVEL, &nLogLevel)){
		fprintf(stderr, "log_level error\n");
	}

	if (!config_lookup_string(cf, LOG_FILE_PATH, &pszLogFilePath)){
		fprintf(stderr, "log_file_path error\n");
	}

	if (!config_lookup_int(cf, LOG_MAX_SIZE, &nLogMaxSize)){
		fprintf(stderr, "log_max_size error\n");
	}

	if (!config_lookup_int(cf, LOG_BACKUP_NUM, &nLogBackupNum)){
		fprintf(stderr, "log_backup_num error\n");
	}

	pszConfigDirPath = (char *)malloc(PATH_MAX);
	ERRMEMGOTO(pszConfigDirPath, result, _EXIT);

	memcpy(pszConfigDirPath, pszConfigPath, strlen(pszConfigPath)+1);
	dirname(pszConfigDirPath);

	pszLogPath = (char *)malloc(PATH_MAX);
	ERRMEMGOTO(pszLogPath, result, _EXIT);

	//make the full path of log file
	if((pszLogFilePath[0] != '/') && (pszLogFilePath[0] != '.') && (pszLogFilePath[0] != '~')){
		memcpy(pszLogPath, pszConfigDirPath, strlen(pszConfigDirPath)+1);
		strncat(pszLogPath, "/", 1);
		strncat(pszLogPath,pszLogFilePath, strlen(pszLogFilePath)+1);
	}
	else{
		realpath(pszLogFilePath, pszLogPath);
	}

	pstConfigData->pszBrokerURI = strdup(pszBrokerURI);
	pstConfigData->nLogLevel = nLogLevel;

	pstConfigData->strLogFilePath = CAPString_New();
	ERRMEMGOTO(pstConfigData->strLogFilePath, result, _EXIT);

	result = CAPString_SetLow(pstConfigData->strLogFilePath, pszLogPath,strlen(pszLogPath)+1);
	ERRIFGOTO(result, _EXIT);

	pstConfigData->nLogMaxSize = nLogMaxSize*MB;
	pstConfigData->nLogBackupNum = nLogBackupNum;
	
	SAFEMEMFREE(pszConfigDirPath);
	SAFEMEMFREE(pszLogPath);

	config_destroy(cf);

	result = ERR_CAP_NOERROR;
_EXIT:
	return result;
}

int main(int argc, char* argv[])
{
    cap_handle hCentralManager = NULL;
    SConfigData *pstConfigData = NULL;
    cap_result result = ERR_CAP_UNKNOWN;
	char* config_file = NULL;
	cap_string strLogPrefix = NULL;

    if(argc != 2){
        fprintf(stderr, "Usage : ./cap_iot_middleware [PATH_OF_CONFIGURATION_FILE]\n");
        return -1;
    }

    signal(SIGPIPE, SIG_IGN);

    pstConfigData = (SConfigData*)malloc(sizeof(SConfigData));
	ERRMEMGOTO(pstConfigData, result, _EXIT);

    result = getConfigData(pstConfigData, argv[1]);
	ERRIFGOTO(result, _EXIT);

	strLogPrefix = CAPString_New();
	ERRMEMGOTO(strLogPrefix, result, _EXIT);

	result = CAPString_SetLow(strLogPrefix, "conviot_middleware", CAPSTRING_MAX);
	ERRIFGOTO(result, _EXIT);
	
	result = CAPLogger_Create(pstConfigData->strLogFilePath, strLogPrefix, pstConfigData->nLogLevel, pstConfigData->nLogMaxSize, pstConfigData->nLogBackupNum, &g_hLogger);
	if( result == ERR_CAP_OPEN_FAIL ){
		printf("Can not open a log file. %s \n", CAPString_LowPtr(pstConfigData->strLogFilePath, NULL));
	}
	ERRIFGOTO(result, _EXIT);
		
	result = CAPLogger_Write(g_hLogger, MSG_INFO, "conviot_middleware start");
	ERRIFGOTO(result, _EXIT);
    
	result = CentralManager_Create(&hCentralManager, pstConfigData->pszBrokerURI, NULL, 0);
    ERRIFGOTO(result, _EXIT);

    result = CentralManager_Execute(hCentralManager, pstConfigData->pszBrokerURI);
    ERRIFGOTO(result, _EXIT);

	result = CentralManager_Destroy(&hCentralManager);
    ERRIFGOTO(result, _EXIT);

    SAFEMEMFREE(pstConfigData->pszBrokerURI);
	SAFE_CAPSTRING_DELETE(pstConfigData->strLogFilePath);
	SAFE_CAPSTRING_DELETE(strLogPrefix);
    SAFEMEMFREE(pstConfigData);
 
_EXIT:
	CAPLogger_Write(g_hLogger, MSG_INFO, "conviot_middlware stop");
	result = CAPLogger_Destroy(&g_hLogger);    
	return 0;
}
