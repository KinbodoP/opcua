/*************************************************************************\
* Copyright (c) 2018 ITER Organization.
* This module is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <ralph.lange@gmx.de>
 *
 *  based on prototype work by Bernhard Kuner <bernhard.kuner@helmholtz-berlin.de>
 */

#include <iostream>
#include <string>
#include <stdexcept>

#include <iocsh.h>
#include <errlog.h>
#include <epicsThread.h>

#include <epicsExport.h>  // defines epicsExportSharedSymbols
#include "Session.h"
#include "RecordConnector.h"
#include "SessionUaSdk.h"
#include "SubscriptionUaSdk.h"
#include "DataElement.h"
#include "iocshVariables.h"

namespace {

using namespace DevOpcua;

static const iocshArg opcuaShowSessionArg0 = {"session name", iocshArgString};
static const iocshArg opcuaShowSessionArg1 = {"verbosity", iocshArgInt};

static const iocshArg *const opcuaShowSessionArg[2] = {&opcuaShowSessionArg0, &opcuaShowSessionArg1};

static const iocshFuncDef opcuaShowSessionFuncDef = {"opcuaShowSession", 2, opcuaShowSessionArg};

static
void opcuaShowSessionCallFunc (const iocshArgBuf *args)
{
    try {
        if (args[0].sval == nullptr || args[0].sval[0] == '\0') {
            SessionUaSdk::showAll(args[1].ival);
        } else {
            SessionUaSdk::findSession(args[0].sval).show(args[1].ival);
        }
    }
    catch (std::exception &e) {
        std::cerr << "ERROR : " << e.what() << std::endl;
    }
}

static const iocshArg opcuaConnectArg0 = {"session name", iocshArgString};

static const iocshArg *const opcuaConnectArg[1] = {&opcuaConnectArg0};

static const iocshFuncDef opcuaConnectFuncDef = {"opcuaConnect", 1, opcuaConnectArg};

static
void opcuaConnectCallFunc (const iocshArgBuf *args)
{
    bool ok = true;

    if (args[0].sval == nullptr) {
        errlogPrintf("ERROR : missing argument #1 (session name)\n");
        ok = false;
    }

    if (ok) {
        try {
            DevOpcua::Session &s = DevOpcua::Session::findSession(args[0].sval);
            s.connect();
        }
        catch (std::exception &e) {
            std::cerr << "ERROR : " << e.what() << std::endl;
        }
    }
}

static const iocshArg opcuaDisconnectArg0 = {"session name", iocshArgString};

static const iocshArg *const opcuaDisconnectArg[1] = {&opcuaDisconnectArg0};

static const iocshFuncDef opcuaDisconnectFuncDef = {"opcuaDisconnect", 1, opcuaDisconnectArg};

static
void opcuaDisconnectCallFunc (const iocshArgBuf *args)
{
    bool ok = true;

    if (args[0].sval == nullptr) {
        errlogPrintf("ERROR : missing argument #1 (session name)\n");
        ok = false;
    }

    if (ok) {
        try {
            DevOpcua::Session &s = DevOpcua::Session::findSession(args[0].sval);
            s.disconnect();
        }
        catch (std::exception &e) {
            std::cerr << "ERROR : " << e.what() << std::endl;
        }
    }
}

static const iocshArg opcuaDebugSessionArg0 = {"session name [\"\"=all]", iocshArgString};
static const iocshArg opcuaDebugSessionArg1 = {"debug level [0]", iocshArgInt};

static const iocshArg *const opcuaDebugSessionArg[2] = {&opcuaDebugSessionArg0, &opcuaDebugSessionArg1};

static const iocshFuncDef opcuaDebugSessionFuncDef = {"opcuaDebugSession", 2, opcuaDebugSessionArg};

static
void opcuaDebugSessionCallFunc (const iocshArgBuf *args)
{
    try {
        Session::findSession(args[0].sval).debug = args[1].ival;
    }
    catch (std::exception &e) {
        std::cerr << "ERROR : " << e.what() << std::endl;
    }
}

static const iocshArg opcuaCreateSubscriptionArg0 = {"subscription name", iocshArgString};
static const iocshArg opcuaCreateSubscriptionArg1 = {"session name", iocshArgString};
static const iocshArg opcuaCreateSubscriptionArg2 = {"publishing interval (ms)", iocshArgDouble};
static const iocshArg opcuaCreateSubscriptionArg3 = {"priority [0]", iocshArgInt};
static const iocshArg opcuaCreateSubscriptionArg4 = {"debug level [0]", iocshArgInt};

static const iocshArg *const opcuaCreateSubscriptionArg[5] = {&opcuaCreateSubscriptionArg0, &opcuaCreateSubscriptionArg1,
                                                              &opcuaCreateSubscriptionArg2, &opcuaCreateSubscriptionArg3,
                                                              &opcuaCreateSubscriptionArg4};

static const iocshFuncDef opcuaCreateSubscriptionFuncDef = {"opcuaCreateSubscription", 5, opcuaCreateSubscriptionArg};

static
void opcuaCreateSubscriptionCallFunc (const iocshArgBuf *args)
{
    try {
        bool ok = true;
        int debuglevel = 0;
        double publishingInterval = 0.;
        epicsUInt8 priority = 0;

        if (args[0].sval == nullptr) {
            errlogPrintf("missing argument #1 (subscription name)\n");
            ok = false;
        } else if (strchr(args[0].sval, ' ')) {
            errlogPrintf("invalid argument #1 (subscription name) '%s'\n",
                         args[0].sval);
            ok = false;
        } else if (Subscription::subscriptionExists(args[0].sval)) {
            errlogPrintf("subscription name %s already in use\n",
                         args[0].sval);
            ok = false;
        }

        if (args[1].sval == nullptr) {
            errlogPrintf("missing argument #2 (session name)\n");
            ok = false;
        } else if (strchr(args[1].sval, ' ')) {
            errlogPrintf("invalid argument #2 (session name) '%s'\n",
                         args[1].sval);
            ok = false;
        } else if (!Session::sessionExists(args[1].sval)) {
            errlogPrintf("session %s does not exist\n",
                         args[1].sval);
            ok = false;
        }

        if (args[2].dval < 0) {
            errlogPrintf("invalid argument #3 (publishing interval) '%f'\n",
                         args[2].dval);
            ok = false;
        } else if (args[2].dval == 0) {
            publishingInterval = opcua_DefaultPublishInterval;
        } else {
            publishingInterval = args[2].dval;
        }

        if (args[3].ival < 0 || args[3].ival > 255) {
            errlogPrintf("invalid argument #4 (priority) '%d'\n",
                         args[3].ival);
        } else {
            priority = static_cast<epicsUInt8>(args[3].ival);
        }

        if (args[4].ival < 0) {
            errlogPrintf("invalid argument #5 (debug level) '%d'\n",
                         args[4].ival);
        } else {
            debuglevel = args[4].ival;
        }

        if (ok) {
            new SubscriptionUaSdk(args[0].sval, &SessionUaSdk::findSession(args[1].sval),
                    publishingInterval, priority, debuglevel);
            if (debuglevel)
                errlogPrintf("opcuaCreateSubscriptionUaSdk: successfully configured subscription '%s'\n", args[0].sval);
        } else {
            errlogPrintf("ERROR - no subscription created\n");
        }
    }
    catch(std::exception& e) {
        std::cerr << "ERROR : " << e.what() << std::endl;
    }
}

static const iocshArg opcuaShowSubscriptionArg0 = {"subscription name", iocshArgString};
static const iocshArg opcuaShowSubscriptionArg1 = {"verbosity", iocshArgInt};

static const iocshArg *const opcuaShowSubscriptionArg[2] = {&opcuaShowSubscriptionArg0, &opcuaShowSubscriptionArg1};

static const iocshFuncDef opcuaShowSubscriptionFuncDef = {"opcuaShowSubscription", 2, opcuaShowSubscriptionArg};

static
void opcuaShowSubscriptionCallFunc (const iocshArgBuf *args)
{
    try {
        if (args[0].sval == nullptr || args[0].sval[0] == '\0') {
            SubscriptionUaSdk::showAll(args[1].ival);
        } else {
            SubscriptionUaSdk::findSubscription(args[0].sval).show(args[1].ival);
        }
    }
    catch (std::exception &e) {
        std::cerr << "ERROR : " << e.what() << std::endl;
    }
}

static const iocshArg opcuaShowDataArg0 = {"record name", iocshArgString};
static const iocshArg opcuaShowDataArg1 = {"verbosity", iocshArgInt};

static const iocshArg *const opcuaShowDataArg[2] = {&opcuaShowDataArg0, &opcuaShowDataArg1};

static const iocshFuncDef opcuaShowDataFuncDef = {"opcuaShowData", 2, opcuaShowDataArg};

static
void opcuaShowDataCallFunc (const iocshArgBuf *args)
{
    try {
        if (args[0].sval == NULL || args[0].sval[0] == '\0') {
            errlogPrintf("missing argument #1 (record name)\n");
        } else {
            RecordConnector *rc = RecordConnector::findRecordConnector(args[0].sval);
            if (rc) {
                rc->pitem->show(1);
            } else {
                errlogPrintf("record %s does not exist\n",
                             args[0].sval);
            }
        }
    }
    catch (std::exception &e) {
        std::cerr << "ERROR : " << e.what() << std::endl;
    }
}

static const iocshArg opcuaSetNamespaceUriArg0 = { "Session name", iocshArgString };
static const iocshArg opcuaSetNamespaceUriArg1 = { "URI", iocshArgString };
static const iocshArg opcuaSetNamespaceUriArg2 = { "Namespace Index", iocshArgInt };
static const iocshArg *const opcuaSetNamespaceUriArg[3] = { &opcuaSetNamespaceUriArg0,&opcuaSetNamespaceUriArg1,&opcuaSetNamespaceUriArg2 };
iocshFuncDef opcuaSetNamespaceUriFuncDef = { "opcuaSetNamespaceUri", 3, opcuaSetNamespaceUriArg };

void opcuaSetNamespaceUriCallFunc(const iocshArgBuf *args)
{
    UaString g_sessionName;
    UaString g_namespaceUri;

    unsigned int g_namespaceIdx;
    if (args[0].sval == NULL){
        errlogPrintf("opcuaSetNamespaceUri: ABORT Missing Argument \"Session name\".\n");
        return;
    }
    if (args[1].sval == NULL){
        errlogPrintf("opcuaSetNamespaceUri: ABORT Missing Argument \"URI\".\n");
        return;
    }

    if (args[2].ival == NULL){
        errlogPrintf("opcuaSetNamespaceUri: ABORT Missing Argument \"Namespace Index\".\n");
        return;
    }
    if ((args[2].ival < 0) || (args[2].ival > std::numeric_limits<unsigned int>::max())) {
        errlogPrintf("opcuaSetNamespaceUri: ABORT Invalid Argument \"Namespace Index\". Expected unsigned int.\n");
        return;
    }
    g_sessionName = args[0].sval;
    g_namespaceUri = args[1].sval;
    g_namespaceIdx = args[2].ival;

    try {
        SessionUaSdk &s = SessionUaSdk::findSession(args[0].sval);

        if ((s.originalUriTable.length() < g_namespaceIdx + 1) || (g_namespaceIdx < 0)) {
            errlogPrintf("opcuaSetNamespaceUri: \t Namespace index %d exceeds maximum allowed index.\n", g_namespaceIdx);
            return;
        }
        s.originalUriTable[g_namespaceIdx] = *g_namespaceUri.copy();
        if (s.isConnected()) 
            s.updateNamespaceIndexes();
        else
            errlogPrintf("opcuaSetNamespaceUri: \t Session '%s' is not connected. Namespace indexes will be updated when connection is established\n", g_sessionName.toUtf8());

    }
    catch (std::exception &e) {
        std::cerr << "ERROR : " << e.what() << std::endl;
        errlogPrintf("opcuaSetNamespaceUri: \t couldnt find session with name %s\n", g_sessionName.toUtf8());
    }
}

static
void opcuaUaSdkIocshRegister ()
{
    iocshRegister(&opcuaConnectFuncDef, opcuaConnectCallFunc);
    iocshRegister(&opcuaDisconnectFuncDef, opcuaDisconnectCallFunc);
    iocshRegister(&opcuaShowSessionFuncDef, opcuaShowSessionCallFunc);
    iocshRegister(&opcuaDebugSessionFuncDef, opcuaDebugSessionCallFunc);

    iocshRegister(&opcuaCreateSubscriptionFuncDef, opcuaCreateSubscriptionCallFunc);
    iocshRegister(&opcuaShowSubscriptionFuncDef, opcuaShowSubscriptionCallFunc);

    iocshRegister(&opcuaShowDataFuncDef, opcuaShowDataCallFunc);
    iocshRegister(&opcuaSetNamespaceUriFuncDef, opcuaSetNamespaceUriCallFunc);
}

extern "C" {
epicsExportRegistrar(opcuaUaSdkIocshRegister);
}

} // namespace
