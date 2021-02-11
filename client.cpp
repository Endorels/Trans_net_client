#include "client.h"

TransportClient::TransportClient(QObject * parent) : QObject(parent)
{
    this->setParent(parent);

    // Reading configuration file
    read_config();

    size_of_header = sizeof(shell_message) - sizeof(QByteArray) - 2;

    // Reading the network addresses of the computer
    QNetworkInterface eth1Ip = QNetworkInterface::interfaceFromName("eth1");
    QList<QNetworkAddressEntry> entries_ip1 = eth1Ip.addressEntries();
    if(!entries_ip1.isEmpty())
    {
        QNetworkAddressEntry entry1 = entries_ip1.first();
        ip1_computer_this = entry1.ip().toString();
    }
    QNetworkInterface eth2Ip = QNetworkInterface::interfaceFromName("eth2");
    QList<QNetworkAddressEntry> entries_ip2 = eth2Ip.addressEntries();
    if(!entries_ip2.isEmpty())
    {
        QNetworkAddressEntry entry2 = entries_ip2.first();
        ip2_computer_this = entry2.ip().toString();
    }

    //  Timer to check the connection with the server
    timer_check_connection = new QTimer(this);
    timer_check_connection->setInterval(1000);
    connect(timer_check_connection, &QTimer::timeout, this, &TransportClient::check_for_network);

    //  Timer for deleting irrelevant data from the buffer
    timer_clear_live_time = new QTimer(this);
    timer_clear_live_time->setInterval(1000);
    connect(timer_clear_live_time, &QTimer::timeout, this, &TransportClient::check_old_messages);
    timer_clear_live_time->start();

    // Timer for sending the first message
    timer_first_connection = new QTimer(this);
    timer_first_connection->setInterval(3000);
    connect(timer_first_connection, &QTimer::timeout, this, &TransportClient::connect_try);
    timer_first_connection->start();

    // Connects for sockets
    socket_this = new QTcpSocket(this);
    connect(socket_this, &QTcpSocket::readyRead, this, &TransportClient::read_message);

    // Connect by connecting to the server by timeout
    connect(this, &TransportClient::send_my_name_to_server, this, &TransportClient::send_my_name);

}

void TransportClient::create_data_mess(const QByteArray *receiver, QByteArray &message, const int &time, const int &part, const int &part_summ)
{
    QByteArray my_info_mess = message;
    QByteArray info;
    int size = my_info_mess.size();
    QByteArray out_name_sender = name_computer_this.toUtf8();
    int name_send_s = out_name_sender.size();

    QByteArray out_name_receiver =  *receiver;
    int name_s = out_name_receiver.size();

    char nul_char = 0;

    QDataStream out_s(&info,QIODevice::WriteOnly);
    out_s.writeRawData(sep_message,3);
    out_s << static_cast<unsigned short int>(1); // mess_type
    out_s << static_cast<unsigned short int>(number_of_message);
    out_s << static_cast<unsigned short int>(time);
    out_s << static_cast<unsigned short int>(part_summ);
    out_s << static_cast<unsigned short int>(part);
    out_s << static_cast<unsigned int>(size);

    out_s.writeRawData(out_name_sender,name_send_s);
    for(int i = 0; i < 10-name_send_s; ++i)
        out_s.writeRawData(&nul_char,1);

    out_s.writeRawData(out_name_receiver,name_s);
    for(int i = 0; i < 10-name_s; ++i)
        out_s.writeRawData(&nul_char,1);

    out_s.writeRawData(my_info_mess,size);

    message = info;
}

void TransportClient::create_regular_mess(const QByteArray *reg_mess, QByteArray &message, const int &part, const int &part_summ, const int &num)
{
    QByteArray info;
    //s.setVersion(QDataStream::Qt_5_13);

    char nul_char = 0;
    int size = reg_mess->size();

    QByteArray out_name_sender = name_computer_this.toUtf8();
    int name_send_size = out_name_sender.size();

    QByteArray out_name_receiver = "Server";
    int name_receiver_size = out_name_receiver.size();

    QDataStream out_s(&info,QIODevice::WriteOnly);
    out_s.writeRawData(sep_message,3);
    out_s << static_cast<unsigned short int>(0);
    out_s << static_cast<unsigned short int>(num);
    out_s << static_cast<unsigned short int>(5);
    out_s << static_cast<unsigned short int>(part_summ);
    out_s << static_cast<unsigned short int>(part);
    out_s << static_cast<unsigned int>(size);

    out_s.writeRawData(out_name_sender,name_send_size);

    for(int i = 0; i < 10-name_send_size; ++i)
        out_s.writeRawData(&nul_char,1);

    out_s.writeRawData(out_name_receiver,name_receiver_size);

    for(int i = 0; i < 10-name_receiver_size; ++i)
        out_s.writeRawData(&nul_char,1);

    out_s.writeRawData(*reg_mess,size);

    message = info;
}

void TransportClient::send_my_name(const QString &name)
{
    if(isFirstMessSended == false)
    {
        socket_this->connectToHost(ip1_serv, port_serv);
        socket_this->waitForConnected(1500);

        if(socket_this->state() == QAbstractSocket::ConnectedState)
        {
            qDebug() << "The connection is established on the eth1!" ;
            QString first_info = QString("My name is %1").arg(name);
            QByteArray first_info_byte = first_info.toUtf8();
            QByteArray first_info_array;
            create_regular_mess(&first_info_byte,first_info_array,1,1,0);
            size_of_header = first_info_array.size() - first_info_byte.size();

            if(socket_this->write(first_info_array) == -1)
                qDebug() << "Error writing to socket - " << socket_this->errorString();

            isFirstMessSended = true;
            timer_check_connection->start();
            timer_first_connection->stop();

            writing_in_socket(); // after reconnect we send the entire saved buffer

        }
        else if((socket_this->errorString() == "Host unreachable") ||
                (socket_this->errorString() == "Connection refused") ||
                (socket_this->errorString() == "Unknown error") ||
                (socket_this->errorString() == "Socket operation timed out"))
        {
            socket_this->abort();
            socket_this->connectToHost(ip2_serv, port_serv);
            socket_this->waitForConnected(1500);

            if(socket_this->state() == QAbstractSocket::ConnectedState)
            {
                qDebug() << "The connection is established on the eth2!" ;
                QString first_info = QString("My name is %1").arg(name);
                QByteArray first_info_byte = first_info.toUtf8();
                QByteArray first_info_array;
                create_regular_mess(&first_info_byte,first_info_array,1,1,0);
                size_of_header = first_info_array.size() - first_info_byte.size();

                if(socket_this->write(first_info_array) == -1)
                    qDebug() << "Error writing to socket - " << socket_this->errorString();

                isFirstMessSended = true;
                timer_check_connection->start();
                timer_first_connection->stop();

                writing_in_socket(); // after reconnect we send the entire saved buffer

            }

            else if((socket_this->errorString() == "Host unreachable") ||
                    (socket_this->errorString() == "Connection refused") ||
                    (socket_this->errorString() == "Unknown error") ||
                    (socket_this->errorString() == "Socket operation timed out"))
            {
                qDebug()  <<"Failed to connect to server ..." ;
                socket_this->abort();

            }
        }
    }
}

void TransportClient::send_message(QByteArray receiver_name, QByteArray info, int time)
{
    int byte_summ = info.size();

    if(number_of_message > num_limit) number_of_message = 1;

    if(byte_summ > max_size_of_message)         // if the sum of bytes is greater than the maximum possible
    {
        double byte_summary = byte_summ;
        int pow = qCeil((byte_summary/max_size_of_message));    //  break into several parts

        QByteArray data = info;

        for(int i = 0; i < pow; ++i)
        {
            QByteArray mess_data = "";
            mess_data = data.mid((i*max_size_of_message),max_size_of_message);

            create_data_mess(&receiver_name,mess_data,time,i+1,pow);        // add service information to the beginning of the message
            pack_message_for_buffer(receiver_name,mess_data,time,i+1,pow);      // we write all the information into the structure

            if(socket_this->state() == QAbstractSocket::ConnectedState)
                if(socket_this->write(mess_data) == -1)
                    qDebug() << "Error writing to socket - " << socket_this->errorString();
        }
    }
    else                                //  if the sum of bytes in the message is less than the maximum, then we transmit
    {
        create_data_mess(&receiver_name,info,time,1,1);         // add service information to the beginning of the message
        pack_message_for_buffer(receiver_name,info,time,1,1);       // we write all the information into the structure
        if(number_of_message > num_limit) number_of_message = 0;

        if(socket_this->state() == QAbstractSocket::ConnectedState)
            if(socket_this->write(info) == -1)
                qDebug() << "Error writing to socket - " << socket_this->errorString();
    }

    number_of_message += 1;
}

void TransportClient::read_message()
{
    connection_check_message = 0;     //  renew value for timer disconnect cause we getting some bytes

    if(buffer_cutted.size() > 0)
    {
        if(socket_this->bytesAvailable() >= buffer_cutted.last().size_in)
        {
            buffer_cutted.last().message = socket_this->read(buffer_cutted.last().size_in);
            message_checking(buffer_cutted.last());
            buffer_cutted.pop_back();
            read_message();
        }
        else return;
    }
    else
    {
        while(socket_this->bytesAvailable() >= size_of_header)
        {
            QByteArray info = socket_this->read(size_of_header);

            if(info.contains(sep_message))
            info.remove(0,sep_message.size());

            shell_message pack_mess;
            creating_shell_struct(&info,pack_mess);  //create a structure for deserialization

            if(socket_this->bytesAvailable() >= pack_mess.size_in)
            {
                pack_mess.message = socket_this->read(pack_mess.size_in);
                message_checking(pack_mess);
            }
            else
            {
                buffer_cutted.push_back(pack_mess);
                break;
            }
        }
    }
}

void TransportClient::message_checking(shell_message &pack_mess)
{
    if(pack_mess.type == 0)
    {
        if(pack_mess.message.contains(control_get_message))   //  get OK - delete message from buffer
        {
            clear_buffer_out(pack_mess);
        }
        else if(pack_mess.message == "1")                  //  request for resend message
        {
            auto iter = buffer_out.begin();
            while(iter != buffer_out.end())
            {
                if(iter->num == pack_mess.num && iter->part_current == pack_mess.part_current)
                {
                    if(socket_this->write(iter->message) == -1)
                        qDebug() << "Error writing to socket - " << socket_this->errorString();
                    break;
                }
                iter++;
            }
        }
    }
    else if( pack_mess.type != 0)            //  information message == 1
    {
        // we don't need dublicates
        bool isContains = false;
        auto iter = buffer_in.begin();
        while(iter != buffer_in.end())
        {
            if(iter->num == pack_mess.num && iter->parts_summ == pack_mess.parts_summ && iter->part_current == pack_mess.part_current)
            {
                isContains = true;
                break;
            }
            else iter++;
        }

        if(isContains) return;

//        check_successive_message_arrival(pack_mess);

        buffer_in.push_back(pack_mess);
        sending_message_ok_message(&pack_mess);   // send OK to Server

        if(pack_mess.part_current < pack_mess.parts_summ)         // if its not last part
        {
            QVector<int> count;
            count.resize(pack_mess.parts_summ);
            count.fill(0);

            if(is_all_parts_here(&pack_mess, count))
            {
                QByteArray final_message = connect_all_parts_together(&pack_mess);

                emit send_message_to_main_prog(&pack_mess.name_of_sender,&final_message);

                clear_buffer_in(pack_mess.num,pack_mess.name_of_sender);
                qDebug() << "Submitting to the main program :: " << final_message;

            }
        }
        else if(pack_mess.part_current == pack_mess.parts_summ)        // if its last part check buffer for all parts exist
        {
            if(pack_mess.parts_summ == 1)       // send to main programm
            {
                QByteArray message_final = pack_mess.message;
                emit send_message_to_main_prog(&pack_mess.name_of_sender,&message_final);
                qDebug() << "Submitting to the main program :: " << message_final;

                clear_buffer_in(pack_mess.num, pack_mess.name_of_sender);
            }
            else                    //  if it consists of several parts
            {
                QVector<int> count;
                count.resize(pack_mess.parts_summ);
                count.fill(0);

                if(is_all_parts_here(&pack_mess, count))  // all parts here
                {
                    QByteArray final_message = connect_all_parts_together(&pack_mess);

                    emit send_message_to_main_prog(&pack_mess.name_of_sender,&final_message);
                    qDebug() << "Submitting to the main program :: " << final_message;

                    clear_buffer_in(pack_mess.num,pack_mess.name_of_sender);
                }
                else                        // some part lost
                {
                    for(int j = 0; j < pack_mess.parts_summ; ++j)   // check parts
                        if(count[j] != 1)                   // send request for part to Server
                        {
                            QByteArray message;
                            QByteArray code = "1";
                            create_regular_mess(&code,message,j+1, pack_mess.parts_summ,pack_mess.num);
                            if(socket_this->write(message) == -1)
                                qDebug() << "Error writing to socket - " << socket_this->errorString();

                       }
                }
            }
        }
    }
}

void TransportClient::check_for_network()
{
    if(connection_check_message > 3)
    {
        qDebug() << "Disconnecting ... Timed out from server.";
        socket_this->abort();

        connection_check_message = 0;
        isFirstMessSended = false;
        reconnect_host();
    }
    else
    {
        QByteArray my_reg_message;
        create_regular_mess(&control_word,my_reg_message,1,1,0);
        if(socket_this->write(my_reg_message) == -1)
            qDebug() << "Error writing to socket - " << socket_this->errorString();

        connection_check_message += 1;
    }
}

void TransportClient::creating_shell_struct(const QByteArray *info_incom, TransportClient::shell_message &pack_mess)
{
    QDataStream in_s(*info_incom);

    unsigned short int in_mess_type;
    unsigned short int in_num;
    unsigned short int in_live_time;
    unsigned short int in_parts_summ;
    unsigned short int in_that_part;
    QString in_name_receiver;
    QString in_name_sender;
    unsigned int in_data_size_bytes;

    in_s>>in_mess_type;
    in_s>>in_num;
    in_s>>in_live_time;
    in_s>>in_parts_summ;
    in_s>>in_that_part;
    in_s>>in_data_size_bytes;
    in_name_receiver = info_incom->mid(14,10);
    in_name_sender = info_incom->mid(24,10);

    pack_mess.type = static_cast<int>(in_mess_type);
    pack_mess.num = static_cast<int>(in_num);
    pack_mess.lifetime = static_cast<int>(in_live_time);
    pack_mess.parts_summ = static_cast<int>(in_parts_summ);
    pack_mess.part_current = static_cast<int>(in_that_part);
    pack_mess.size_in = static_cast<int>(in_data_size_bytes);
    pack_mess.name_of_sender = in_name_sender;
    pack_mess.name_of_receiver = in_name_receiver;
}

void TransportClient::pack_message_for_buffer(QByteArray receiver, QByteArray &message, const int &time, const int &part, const int &sum_part)
{
    shell_message pack_mess;
    pack_mess.num = number_of_message;
    pack_mess.type = 1;
    pack_mess.part_current = part;
    pack_mess.parts_summ = sum_part;
    pack_mess.lifetime = time;
    pack_mess.name_of_sender = name_computer_this;
    pack_mess.name_of_receiver = receiver;
    pack_mess.size_in = message.size();
    pack_mess.time_checking = 0;
    pack_mess.message = message;

    buffer_out.push_back(pack_mess);
}

void TransportClient::writing_in_socket()
{
    if(buffer_out.size() > 0)
    {
        for(auto index : buffer_out)
        {
            if(socket_this->state() == QAbstractSocket::ConnectedState) // && my_soc->bytesToWrite() < 5000)
            {
                if(socket_this->write(index.message) == -1)
                {
                    qDebug() << "Error writing to socket - " << socket_this->errorString();
                    break;
                }
            }
            else
            {
                qDebug() << "The socket is disconnected - " << index.num;
                break;
            }
        }
    }
}

void TransportClient::clear_buffer_in(const int &num, const QString &sender)
{
    auto iter = buffer_in.begin();
    while(iter != buffer_in.end())
        (iter->num == num && iter->name_of_sender == sender) ? iter = buffer_in.erase(iter) : iter++;
}

void TransportClient::clear_buffer_out(TransportClient::shell_message &pack_mess)
{
    auto iter = buffer_out.begin();
    while(iter != buffer_out.end())
    {
        if((iter->num == pack_mess.num) && (iter->part_current == pack_mess.part_current) && (iter->parts_summ == pack_mess.parts_summ))
        {
            buffer_out.erase(iter);
            break;
        }
        else iter++;
    }
}

void TransportClient::request_for_resending_message(shell_message *pack_mess)
{
    QByteArray mess = "", info = "1";

    create_regular_mess(&info,mess,pack_mess->part_current,pack_mess->parts_summ,pack_mess->num);
    if(socket_this->write(mess) == -1)
        qDebug() << "Error writing to socket - " << socket_this->errorString();
}

bool TransportClient::is_all_parts_here(shell_message * pack_mess, QVector<int> &count)
{
    bool is_ok = true;

    auto iter = buffer_in.begin();
    while(iter != buffer_in.end())
    {
        if(iter->num == pack_mess->num)
            count[iter->part_current - 1] = 1;

        iter++;
    }

    for(int i = 0; i < pack_mess->parts_summ; ++i)
    {
        if(count[i] != 1)
        {
            is_ok = false;
        }
    }
    return is_ok;
}

QByteArray TransportClient::connect_all_parts_together(shell_message *pack_mess)
{
    QByteArray final_message = "";

    for(int j = 0; j < pack_mess->parts_summ; j++)
    {
        auto iter = buffer_in.begin();
        while(iter != buffer_in.end())
        {
            if(iter->part_current == j+1)
            {
                QByteArray my_mess = iter->message;
                final_message += my_mess;
            }
            iter++;
        }
    }
    return  final_message;
}

void TransportClient::sending_message_ok_message(shell_message *pack_mess)
{
    QString reg_mess_str = QString("Message OK %1").arg(pack_mess->num);
    QByteArray message;
    QByteArray reg_mess_ba = reg_mess_str.toUtf8();
    create_regular_mess(&reg_mess_ba,message,pack_mess->part_current,pack_mess->parts_summ,pack_mess->num);
    if(socket_this->write(message) == -1)
        qDebug() << "Error writing to socket - " << socket_this->errorString();
}

void TransportClient::show_shell_struct(const TransportClient::shell_message &pack_mess)
{
    //for debug - print shell struct
    qDebug() << "pack_mess.num == " << pack_mess.num;
    qDebug() << "pack_mess.that_part == " << pack_mess.part_current;
    qDebug() << "pack_mess.parts_summ == " << pack_mess.parts_summ;
    qDebug() << "pack_mess.in_name_sender == " << pack_mess.name_of_sender;
    qDebug() << "pack_mess.in_name_receiver == " << pack_mess.name_of_receiver;
    qDebug() << "pack_mess.mess_type == " << pack_mess.type;
    qDebug() << "pack_mess.live_time == " << pack_mess.lifetime;
    qDebug() << "pack_mess.size_in == " << pack_mess.size_in;
}

void TransportClient::read_config()
{
    QRegExp reg_ip("^(\\d|[1-9]\\d|1\\d\\d|2([0-4]\\d|5[0-5]))\\.(\\d|[1-9]\\d|1\\d\\d|2([0-4]\\d|5[0-5]))\\.(\\d|[1-9]\\d|1\\d\\d|2([0-4]\\d|5[0-5]))\\.(\\d|[1-9]\\d|1\\d\\d|2([0-4]\\d|5[0-5]))$");

    QString path = QDir::currentPath();
    path += "/TransportClient/config.txt";
    QFile file(path);

    if (file.open(QIODevice::ReadOnly))
    {
        qDebug() << "File opened successfully.";
        while (!file.atEnd())
        {
            QString s;
            s = file.readLine();

            if(s.contains("This Computer Name"))
            {
                QString line = file.readLine();

                if(line.contains("\r\n"))
                    line.remove("\r\n");
                else if(line.contains("\n"))
                    line.remove("\n");

                name_computer_this = line;
            }
            else if(s.contains("Server IP_1"))
            {
                QString line = file.readLine();

                if(line.contains("\r\n"))
                    line.remove("\r\n");
                else if(line.contains("\n"))
                    line.remove("\n");

                ip1_serv = line;
                if(!ip1_serv.contains(reg_ip))
                    qDebug() << "Invalid IP address Server IP_1 !";
            }
            else if(s.contains("Server IP_2"))
            {
                QString line = file.readLine();

                if(line.contains("\r\n"))
                    line.remove("\r\n");
                else if(line.contains("\n"))
                    line.remove("\n");

                ip2_serv = line;
                if(!ip2_serv.contains(reg_ip))
                    qDebug() << "Invalid IP address Server IP_2 !";
            }
            else if(s.contains("Server Port"))
            {
                QString line = file.readLine();

                if(line.contains("\r\n"))
                    line.remove("\r\n");
                else if(line.contains("\n"))
                    line.remove("\n");

                port_serv = static_cast<quint16>(line.toInt());
            }
        }

        if(ip1_serv != "" && ip2_serv != "" && name_computer_this != "" && port_serv != 0)
            qDebug() << "Config file was successfully read and written! ";
        else qDebug() << "Config file have errors! Programm would work incorrect!";
    }
    file.close();
}

void TransportClient::check_successive_message_arrival(shell_message &pack_mess)
{
    // check for sequential messages
    if(NUMBER == pack_mess.num)
    {
        if(pack_mess.part_current == pack_mess.parts_summ)
        NUMBER++;
    }
    else if((pack_mess.num < NUMBER) || (pack_mess.num > NUMBER))
    {
        NUMBER = pack_mess.num+1;
        qDebug() << "Inconsistent message - " << pack_mess.num;
    }
}

void TransportClient::reconnect_host()
{
    qDebug() << "Attempting to reconnect to the server ...";
    send_my_name(name_computer_this);
}

void TransportClient::check_old_messages()
{
    auto iter = buffer_out.begin();
    while(iter != buffer_out.end())
    {
        if(iter->time_checking >= iter->lifetime)
        {
            iter = buffer_out.erase(iter);
        }
        else
        {
            iter->time_checking++;
            iter++;
        }
    }

    auto iter2 = buffer_in.begin();
    while(iter2 != buffer_in.end())
    {
        if(iter2->time_checking >= iter2->lifetime)
        {
            iter2 = buffer_out.erase(iter2);
        }
        else
        {
            iter2->time_checking++;
            iter2++;
        }
    }
}

void TransportClient::connect_try()
{
    emit send_my_name_to_server(name_computer_this);
}
