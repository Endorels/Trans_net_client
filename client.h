#ifndef TRANSPORTCLIENT_H
#define TRANSPORTCLIENT_H

#include "Client_global.h"
#include <QObject>
#include <QDir>
#include <QRegExp>
#include <QFile>
#include <QTcpSocket>
#include <QNetworkInterface>
#include <QTime>
#include <QTimer>
#include <QStringList>
#include <QDataStream>
#include <QtMath>
#include <QLinkedList>
#include <QList>
#include <QDebug>
#include <QCoreApplication>

// /*******************     Structure of message   **********************************************************************************************************************/
// !@# | type | num | lifetime | parts_summ | part_curr | size_in | sender_name | receiver_name | message
// /*********************************************************************************************************************************************************************/

class TRANSPORTCLIENT_EXPORT TransportClient : public QObject
{
    Q_OBJECT
public:
    explicit TransportClient(QObject *parent = nullptr);
    ~TransportClient() {/*qDebug() << "Transport cliend deleted!";*/}

private:

    struct shell_message{
        int type;                  //  message type
        int num;                   //  message number
        int lifetime;             //  message lifetime
        int parts_summ;            //  total number of message parts
        int part_current;          //  number of the current part of the message
        int size_in;               //  number of bytes in an information message
        QString name_of_receiver;  //  receiver name
        QString name_of_sender;    //  sender name
        int time_checking;         //  variable for calculating message lifetime
        QByteArray message;        //  full message with shell

    };

    QString ip1_serv = "";
    QString ip2_serv = "";
    quint16 port_serv = 0;
    QString name_computer_this = "";
    QString ip1_computer_this = "";
    QString ip2_computer_this = "";

    QTcpSocket * socket_this = nullptr;

    const int num_limit = 30000;        //  message number limit
    bool isFirstMessSended = false;     //  variable to track the delivery of the first message about this programm

    int max_size_of_message = 363;      //  max size of message
    int size_of_header = 37;            //  header size
    int connection_check_message = 0;   //  ping variable
    int number_of_message = 1;


    QLinkedList<shell_message> buffer_in;        //  buffer for saving input messages
    QLinkedList<shell_message> buffer_out;       //  buffer for saving output messages
    QLinkedList<shell_message> buffer_cutted;    //  buffer for incomplete messages

    QByteArray control_answer = "pong";
    QByteArray control_word = "ping";
    QByteArray control_get_message = "Message OK";
    QByteArray sep_message = "!@#";                 // message separator

    QTimer * timer_check_connection;    //  timer to check if there is a connection to the server
    QTimer * timer_first_connection;    //  timer to try to connect to the server for the first time
    QTimer * timer_clear_live_time;     //  timer for clearing the buffer of expired messages


    //  function of creating information messages (type == 1)
    void create_data_mess(const QByteArray * receiver, QByteArray &message,const int &time,const int &part,const int &sum_part);

    //  function to create regular messages (type == 0)
    void create_regular_mess(const QByteArray * reg_mess, QByteArray &message,const int &part,const int &sum_part,const int &num);

    //  message deserialization function
    void creating_shell_struct(const QByteArray * info, shell_message &shell_message);

    //  function for serializing messages and writing to the buffer
    void pack_message_for_buffer(QByteArray receiver, QByteArray &message,const int &time, const int &part, const int &sum_part);

    //  function to write to socket
    void writing_in_socket();

    //  function of clearing incoming buffer by message number
    void clear_buffer_in(const int &num,const QString &sender);

    //  function to clear outgoing buffer by message number
    void clear_buffer_out(shell_message &pack_mess);

    //  function for request to resend a message
    void request_for_resending_message(shell_message * pack_struct);

    // function of checking if all parts are in place
    bool is_all_parts_here(shell_message * income_pack_ptr, QVector<int> &count);

    // function of combining message pieces into one
    QByteArray connect_all_parts_together(shell_message * income_pack_ptr);

    // function of sending confirmation of message receipt
    void sending_message_ok_message(shell_message * pack_mess);

    // config file read function
    void read_config();


    // DEBUG
    int NUMBER = 1;
    void check_successive_message_arrival(shell_message &pack_mess);  // function of checking for sequential arrival of messages
    void show_shell_struct(const shell_message & pack);               // message structure output function

public slots:

    //  slot for sending a message to another subscriber
    void send_message(QByteArray receiver_name, QByteArray  info, int live_time);

private slots:

    //  the function of sending a message to the server about this programm
    void send_my_name(const QString &message);

    //  function of reading messages from the socket
    void read_message();

    //  message handling function
    void message_checking(shell_message &pack);

    //  server connection check function
    void check_for_network();

    // function for checking the lifetime of messages in the buffer
    void check_old_messages();

    //  function of trying to establish connection with the server
    void connect_try();

    //  function of reconnecting to the server
    void reconnect_host();

    signals:

    //  signal to send messages to the main program
    void send_message_to_main_prog(QString  * adress_name, QByteArray * data);

    //  signal about issuing information about the programm to the server
    void send_my_name_to_server(const QString &VK_name);


};

#endif // TRANSPORTCLIENT_H
