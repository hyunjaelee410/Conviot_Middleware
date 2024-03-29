#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define BROKER_URI "broker_uri"
#define LOG_LEVEL "log_level"
#define LOG_FILE_PATH "log_file_path"
#define LOG_MAX_SIZE "log_max_size"
#define LOG_BACKUP_NUM "log_backup_num"
#define SOCKET_LISTENING_PORT "socket_listening_port"
#define ALIVE_CHECKING_PERIOD "alive_checking_period"
#define DB_HOST "db_host"
#define DB_USER "db_user"
#define DB_PASSWORD "db_password"
#define DB_NAME "db_name"
#define DB_PORT "db_port"

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

cap_handle g_hLogger = NULL;

static int getConfigData(SConfigData *pstConfigData, char *pszConfigPath)
{
	cap_result result = ERR_CAP_UNKNOWN;
    const char *pszBrokerURI = NULL;
	int nLogLevel;
	const char *pszLogFilePath = NULL;
	int nLogMaxSize;
	int nLogBackupNum;
    int nSocketListeningPort;
    int nAliveCheckingPeriod = 0;

	config_t cfg, *cf;
	char *pszLogPath = NULL;
	cf = &cfg;
    config_init(cf);

	char *pszConfigDirPath = NULL;
    
    const char *pszDBHost = NULL;
    const char *pszDBUser = NULL;
    const char *pszDBPassword = NULL;
    const char *pszDBName = NULL;
    int nDBPort = 0;

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
    
    if (!config_lookup_int(cf, SOCKET_LISTENING_PORT, &nSocketListeningPort)){
        fprintf(stderr, "socket_listening_port error\n");
        ERRASSIGNGOTO(result, ERR_CAP_INVALID_DATA, _EXIT);
    }   
    
    if (!config_lookup_int(cf, ALIVE_CHECKING_PERIOD, &nAliveCheckingPeriod)){
		fprintf(stderr, "AliveCheckingPeriod error\n");
        ERRASSIGNGOTO(result, ERR_CAP_INVALID_DATA, _EXIT);
	}

    if (!config_lookup_string(cf, DB_HOST, &pszDBHost)){
        fprintf(stderr, "db_host error\n");
    }
    if (!config_lookup_string(cf, DB_USER, &pszDBUser)){
        fprintf(stderr, "db_user error\n");
    }
    if (!config_lookup_string(cf, DB_PASSWORD, &pszDBPassword)){
        fprintf(stderr, "db_password error\n");
    }
    if (!config_lookup_string(cf, DB_NAME, &pszDBName)){
        fprintf(stderr, "db_name error\n");
    }
	if (!config_lookup_int(cf, DB_PORT, &nDBPort)){
		fprintf(stderr, "db_port error\n");
	}

    //TODO
    //Get socket listening and connecting port then hand it to Infomanager

	pszConfigDirPath = (char *)malloc(PATH_MAX);
	ERRMEMGOTO(pszConfigDirPath, result, _EXIT);

	memcpy(pszConfigDirPath, pszConfigPath, strlen(pszConfigPath)+1);
	dirname(pszConfigDirPath);

	pszLogPath = (char *)malloc(PATH_MAX);
	ERRMEMGOTO(pszLogPath, result, _EXIT);

    pstConfigData->nAliveCheckingPeriod = nAliveCheckingPeriod;
	pstConfigData->pszBrokerURI = strdup(pszBrokerURI);
    pstConfigData->nSocketListeningPort = nSocketListeningPort; 
	pstConfigData->nLogLevel = nLogLevel;

	pstConfigData->strLogFilePath = CAPString_New();
	ERRMEMGOTO(pstConfigData->strLogFilePath, result, _EXIT);

	result = CAPString_SetLow(pstConfigData->strLogFilePath, pszLogFilePath,strlen(pszLogFilePath)+1);
	ERRIFGOTO(result, _EXIT);

	pstConfigData->nLogMaxSize = nLogMaxSize*MB;
	pstConfigData->nLogBackupNum = nLogBackupNum;

    pstConfigData->pstDBInfo->pszDBHost = strdup(pszDBHost);
    pstConfigData->pstDBInfo->pszDBUser = strdup(pszDBUser);
    pstConfigData->pstDBInfo->pszDBPassword = strdup(pszDBPassword);
    pstConfigData->pstDBInfo->pszDBName = strdup(pszDBName);
    pstConfigData->pstDBInfo->nDBPort = nDBPort;

	
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
    SDBInfo *pstDBInfo = NULL;
    cap_result result = ERR_CAP_UNKNOWN;
	cap_string strLogPrefix = NULL;
	cap_bool d_opt = FALSE;
	cap_bool f_opt = FALSE;
	char* config_file = NULL;
	int opt;
	int optnum = 0;

	opterr = 0;
	while((opt = getopt(argc, argv, "df:")) != -1) 
	{
		optnum++;
		switch(opt) 
		{ 
			case 'd':
				d_opt = TRUE;
				break; 
			case 'f':
				f_opt = TRUE;
				config_file = optarg;
				optnum++;
				break;
			case '?':		//in the case of unknown options
				printf("Usage: conviot_middleware [-d] [-f configure_file_path]\n");
				return -1;
		} 
	}
	if( (argc > 1) && (optnum == 0)){	//with argvs without '-'  ex)./conviot_middleware argv
		printf("Usage: conviot_middleware [-d] [-f configure_file]\n");
		return -1;
	}

	if( f_opt == FALSE ){
		config_file = "middleware_config.cfg";
	}

    signal(SIGPIPE, SIG_IGN);

    pstConfigData = (SConfigData*)malloc(sizeof(SConfigData));
	ERRMEMGOTO(pstConfigData, result, _EXIT);
    
    pstDBInfo = (SDBInfo*)malloc(sizeof(SDBInfo));
	ERRMEMGOTO(pstDBInfo, result, _EXIT);

    pstConfigData->pstDBInfo = pstDBInfo;

    result = getConfigData(pstConfigData, config_file);
	ERRIFGOTO(result, _EXIT);
	
    if(d_opt == TRUE){
		if(daemon(0,0) == -1){	// demonize a program and is included in unistd.h
			return -1;
		}
	}

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
    
	result = CentralManager_Create(&hCentralManager, pstConfigData);
    ERRIFGOTO(result, _EXIT);

    result = CentralManager_Execute(hCentralManager, pstConfigData);
    ERRIFGOTO(result, _EXIT);

	result = CentralManager_Destroy(&hCentralManager);
    ERRIFGOTO(result, _EXIT);

    SAFEMEMFREE(pstConfigData->pszBrokerURI);
    SAFEMEMFREE(pstDBInfo->pszDBHost);
    SAFEMEMFREE(pstDBInfo->pszDBPassword);
    SAFEMEMFREE(pstDBInfo->pszDBUser);
    SAFEMEMFREE(pstDBInfo->pszDBName);
    SAFEMEMFREE(pstDBInfo);
	SAFE_CAPSTRING_DELETE(pstConfigData->strLogFilePath);
	SAFE_CAPSTRING_DELETE(strLogPrefix);
    SAFEMEMFREE(pstConfigData);
 
_EXIT:
	CAPLogger_Write(g_hLogger, MSG_INFO, "conviot_middlware stop");
	result = CAPLogger_Destroy(&g_hLogger);    
	return 0;
}
