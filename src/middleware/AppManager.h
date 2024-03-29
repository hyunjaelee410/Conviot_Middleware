#ifndef APPMANAGER_H_
#define APPMANAGER_H_

#include <CAPString.h>
#include <CAPThread.h>
#include <mysql.h>

#include <capiot_common.h>

#include <MQTT_common.h>
#include "CentralManager.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef struct _SAppManager {
    EIoTHandleId enID;
    cap_string strBrokerURI;
    cap_bool bCreated;
    cap_handle hMQTTHandler;
    MYSQL *pDBconn;
} SAppManager;

typedef struct _SConditionContext {
    cap_string strExpression;	
    int nConditionId;
    cap_bool bIsSingleCondition;
    cap_bool bIsSatisfied;
    EType enType;
    int nEcaId; //related eca id
    EOperator enEcaOp; //related eca operator
} SConditionContext;

typedef struct _SActionContext {
    cap_string strReceiverId;   //receiver could be either service or device
    cap_string strFunctionName;	
    cap_string strArgumentPayload;	
    cap_bool   bIsServiceType;
    int        nUserId;
} SActionContext;

cap_result AppManager_Create(OUT cap_handle* phAppManager, cap_string strBrokerURI, IN SDBInfo *pstDBInfo);
cap_result AppManager_Run(cap_handle hAppManager);
cap_result AppManager_Join(cap_handle hAppManager);
cap_result AppManager_Destroy(OUT cap_handle* phAppManager);

#ifdef __cplusplus
}
#endif

#endif
