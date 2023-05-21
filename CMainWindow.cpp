#include "CMainWindow.h"
#include "ui_CMainWindow.h"


#ifdef QT_DEBUG
#define mPuts(log)    m_pDebug->PutLog(log)
#else
#define mPuts(log)
#endif


CMainWindow::CMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::CMainWindow)
#ifdef QT_DEBUG
    ,m_pDebug(new CDebug(this))
#endif
    , m_pClient{}
{
    ui->setupUi(this);
#ifdef QT_DEBUG
    m_pDebug->show();
    m_pClient = new CClient(this,m_pDebug,ui->cb_clients);
#else
    m_pClient = new CClient(this,ui->cb_clients);
#endif
    mPuts("CMainWindow::CMainWindow()");
    ui->le_serverIP->setText("127.0.0.1");
    ui->le_serverPort->setText("20000");
    ui->le_identifiant->setText(("Andrew"));

    /* objet qui émet, quel signal, quel objet reçoit, quelle méthode */
    QObject::connect(ui->lw_recvChat->model(),SIGNAL(rowsInserted(QmodelIndex,int,int)),ui->lw_recvChat,SLOT(scrollToBottom()));
    QObject::connect(ui->lw_trmtChat->model(),SIGNAL(rowsInserted(QmodelIndex,int,int)),ui->lw_trmtChat,SLOT(scrollToBottom()));

    QObject::connect(m_pClient,&CClient::PostChatMessage,this,&CMainWindow::on_receivePostChatMessage);

    QObject::connect(m_pClient,SIGNAL(ServerHasDisconnected()),this,SLOT(on_pb_connectDisconnect_clicked()));
}

CMainWindow::~CMainWindow(){
    delete m_pClient;
#ifdef QT_DEBUG
    delete m_pDebug; m_pDebug = nullptr;
#endif
    delete ui;
}

void CMainWindow::keyPressEvent(QKeyEvent*event){
    switch(event->key()){
    case Qt::Key_Escape:
        event->accept();
        QApplication::quit();
        break;
    default:
        event->ignore();
        break;
    }
}

void CMainWindow::on_le_msgChat_returnPressed(){
    mPuts("CMainWindow::on_le_msgChat_returnPressed()");
    m_pClient->SendMessage(ui->le_msgChat->text());
    ui->le_msgChat->clear();
}


void CMainWindow::on_pb_connectDisconnect_clicked(){

    if(m_pClient->IsConnected()){
       if(m_pClient->Disconnect()== 0){
            ui->pb_connectDisconnect->setText("Connect");
       }
       ui->le_serverPort->setReadOnly(false);
       ui->le_serverIP->setReadOnly(false);
       ui->le_identifiant->setReadOnly(false);
    }
    else{
        ui->le_serverPort->setReadOnly(true);
        ui->le_serverIP->setReadOnly(true);
        ui->le_identifiant->setReadOnly(true);

        if(m_pClient->Connect(ui->le_serverIP->text(),
                              ui->le_serverPort->text(),
                              ui->le_identifiant->text()) == 0){
            ui->pb_connectDisconnect->setText("Disconnect");
        }
    }
}

void CMainWindow::AddRecvMessage(const QString& msg){

    ui->lw_recvChat->addItem(msg);
}

void CMainWindow::AddSentMessage(const QString& msg){
    ui->lw_trmtChat->addItem(msg);
}

void CMainWindow::on_pb_ClearRecvChat_clicked(){
    ui->lw_recvChat->clear();
}

void CMainWindow::on_pb_ClearTrmtChat_clicked(){
    ui->lw_trmtChat->clear();
}



void CMainWindow::on_receivePostChatMessage(const QString& msg){
    ui->lw_recvChat->addItem(msg);
}


void CMainWindow::on_receivePostTrmtMessage(const QString& msg){
    ui->lw_trmtChat->addItem(msg);
}
