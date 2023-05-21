#include "CClient.h"
#include<CMainWindow.h>
#include <QComboBox>

using namespace std;

#ifdef QT_DEBUG
#define mPutLog(log)       m_pDebug->PutLog(log)
#define mThPutLog(log)     pClient->m_pDebug->PutLog(log)
#else
#define mPutLog(log)
#define mThPutLog(log)
#endif

#ifdef QT_DEBUG
CClient::CClient(CMainWindow*pMainWindow,CDebug*pDebug,QComboBox* pComboBox):
        m_pDebug{pDebug},
#else
CClient::CClient(CMainWindow*pMainWindow,QComboBox* pComboBox):
#endif
    m_uStatus{ST_ALL_CLEARED},
    m_sock{INVALID_SOCKET},
    m_sin{},
    m_pReceiveThread{},
    m_pMainWindow{pMainWindow},
    m_pComboBox{pComboBox}
{

#if defined (WIN32)
    WSADATA WSAData;
    int error = WSAStartup(MAKEWORD(2,2),&WSAData);
    if(error){
        mBitsSet(m_uStatus,ST_WSADATA_ERROR);
    }
    else{
         mBitsSet(m_uStatus,ST_WSADATA_INITIATED);
    }
#endif
    mPutLog("CClient::CClient()");

}


CClient::~CClient(){
    Disconnect();
#if defined (WIN32)
    if(mBitsSet(m_uStatus,ST_WSADATA_INITIATED)){
            WSACleanup();
        mBitsClr(m_uStatus,ST_WSADATA_INITIATED);
    }
#endif
     mPutLog("CClient::~CClient()");
}


int CClient::Connect(const QString& serverIp,const QString& serverPort,const QString& identifiant){

    if(mIsBitsSet(m_uStatus,ST_LINKED)) return (int)INVALID_SOCKET;

    /* CREATE SOCKET */
    m_sock = socket(AF_INET,SOCK_STREAM,0);
    if(m_sock == INVALID_SOCKET){
        mPutLog("Failed to create socket");
        return (int)INVALID_SOCKET;
    }

    /* Connection configuration */
    m_sin.sin_addr.s_addr = inet_addr(serverIp.toStdString().c_str());
    m_sin.sin_family = AF_INET;
    m_sin.sin_port = htons(serverPort.toShort());

    if(::connect(m_sock,(SOCKADDR*)&m_sin,sizeof m_sin) == SOCKET_ERROR){
        mPutLog("Failed to connect to server !");
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        return (int)INVALID_SOCKET;
    }

    mPutLog(QString("Connexion to ") +inet_ntoa(m_sin.sin_addr) +"on port" + QString::number(htons(m_sin.sin_port)));

    QString pseudo = "/p " + identifiant;
    send(m_sock, pseudo.toStdString().c_str(), pseudo.length()+1,0);
    Sleep(100);
    send(m_sock, "?*", 3,0);
    Sleep(100);
    send(m_sock, "?+", 3,0);

    mBitsSet(m_uStatus,ST_LINKED);

    m_pIdentifiant = identifiant;

    m_pReceiveThread = new ::thread(ReceiveThreadProc,this);
    mPutLog("CClient::Connect()");

    return 0;
}

int CClient::Disconnect(){
    if(mIsBitsClr(m_uStatus, ST_CONNECTED)) return (int)INVALID_SOCKET;

    send(m_sock, "?-", 3,0);
    Sleep(100);
    send(m_sock, "", 1,0);
    m_pComboBox->clear();

    closesocket(m_sock);
    m_sock=INVALID_SOCKET;

    mBitsClr(m_uStatus, ST_CONNECTED);
    m_pReceiveThread->join();
    delete m_pReceiveThread; m_pReceiveThread=nullptr;
    mPutLog("CClient::Disconnect()");
    return 0;
}

int CClient::IsConnected()const{
    return mIsBitsSet(m_uStatus,ST_LINKED);
}

/*static*/ void CClient::ReceiveThreadProc(CClient*pClient){
    mThPutLog("----- CClient::ReceiveThreadProc:: Entering ---- ");

    /* To make the socket unblocking */
    u_long ulMode =1;
    int iResult = ioctlsocket(pClient->m_sock,FIONBIO,&ulMode);
    if(iResult != NO_ERROR){
        mThPutLog("CClient::ReceiveThreadProc() : ioctlsocket failed with an error");
    }

    char buf[BUFSIZ];
    int res = 1;
    int cpt = 0;

    while(res && mIsBitsSet(pClient->m_uStatus,ST_LINKED)){
//        mThPutLog(QString("----- CClient::ReceiveThreadProc:: Running ---- ") + QString::number(cpt++));

        switch(res=recv(pClient->m_sock, buf, sizeof buf, 0)){
        case SOCKET_ERROR:
            this_thread::sleep_for(chrono::duration<int,milli>(100));
            break;
        case 0:
        case 1:
            mThPutLog(QString("Received # ") +QString(buf));
            if(buf[0] == '\0'){
                mThPutLog(QString("Received 0 length string ! : remote server has been disconnected"));
                res = 0;
                emit pClient->ServerHasDisconnected();
            }
            break;
        default:
            switch(buf[0]){
            case '/':
                switch(buf[1]){
                case 'b':   /* Broadcast message to all connected clients */
                    pClient->m_pMainWindow->AddRecvMessage(QString("Received #") + " \""+ (buf) +"\"");
                    break;
                case 'p':   /* Message indicates that a new client is connected */
                    pClient->m_pMainWindow->AddRecvMessage(QString("Received #") + " \""+( buf) + QString(" is connected") + "\"");
                    break;
                case 'm':   /* private message */
                    pClient->m_pMainWindow->AddRecvMessage(QString("Received #") + " \""+ (buf) +"\"");

                    emit pClient->PostChatMessage(QString("EMIT Received #") + " \""+buf+"\"");
                    break;
                }
                break;
            case '?':
                switch(buf[1]){
                case '+':   // Add to the combo Box
                    mThPutLog(QString("Add a client to the comboBox"));
                    pClient->m_pComboBox->addItem(buf+2);
                    break;
                case '-':   // Remove from the combo Box
                    mThPutLog(QString("Remove a client from the comboBox"));
                    pClient->m_pComboBox->removeItem(pClient->m_pComboBox->findText(buf+2));
                    break;
                default:
                    break;
                }
                break;
            default:
                break;

            }
            break;
        }
    }
    if(res == 0)    mThPutLog("----- CClient::ReceiveThreadProc:: Quit the whil function ");

        while(mIsBitsSet(pClient->m_uStatus,ST_LINKED)){;} /* Waiting for local deconnection from the IHm thread */

    mThPutLog("----- CClient::ReceiveThreadProc:: Exiting ---- ");
}


int CClient::SendMessage(const QString& msg)const{

    QString text;
    text = "/m " +QString(m_pIdentifiant) + " : " + QString(msg);
    if(mIsBitsClr(m_uStatus,ST_LINKED)) return -1;
    send(m_sock, text.toStdString().c_str(), text.length()+1,0);

    m_pMainWindow->AddSentMessage(text);

    return 0;
}


int CClient::SendPseudo()const{

    QString text;
    text = "/p " + QString(m_pIdentifiant);
    if(mIsBitsClr(m_uStatus,ST_LINKED)) return -1;
    send(m_sock, text.toStdString().c_str(), text.length()+1,0);

    m_pMainWindow->AddSentMessage(text);

    return 0;
}


//  for(auto it : vec){
//  }

//int CClient::Send(const QString& msg)const{
//    vector<int> vec{0,1,2,3,4,5,6,20};
//    for(vector<int>::const_iterator it = vec.begin(); it != vec.end(); it++){
//        m_pMainWindow->AddSentMessage(QString::number(*it));
//    }
//    return 0;
//}


//  connect with ?+ on rajoute une entrée, ?- on retranche dans la liste, ?* mise à jour de la liste




//            switch(buf[0]){
//            case '?':
//                switch(buf[1]){
//                case '+':   // Add to the combo Box
//                    mThPutLog(QString("Add a client to the comboBox"));
//                    pClient->m_pComboBox->addItem(buf+2);
//                    break;
//                case '-':   // Remove from the combo Box
//                    mThPutLog(QString("Remove a client from the comboBox"));
//                    pClient->m_pComboBox->removeItem(pClient->m_pComboBox->findText(buf+2));
//                    break;
//                default:
//                    break;
//                }
//                break;
//            default:
//                break;
//            }
