#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "telegesis.h"

QString PORTNAME, QCOMMAND;
QStringList qCommandList; //набор команд из командной строки, разделенной ";", наполняется из qCommand
int qCommandIndex=0; //счетчик команд из командной строки
int CONNECT_RESULT; //результат команд из telegesis.h (код возврата)
int LINEBUF[401]; //здесь будем хранить значения для отрисовки графики
QPen PEN(QColor(0,0,0)); //инициализация кисти для рисования графиков
std::ofstream FILE_OUT;
std::ifstream FILE_IN;
int INDEX_OF_WRITE = 0; /*счетчик записанных данных. Будем выводить данные в формате
                          "msec value", где msec - время, прошедшее с запуска,
                          а именно ui->milliseconds->value()* INDEX_OF_WRITE, то есть 100, 200, 300, если таймер стоит
                          на ста миллисекундах*/
int RECIEVED_BYTES=0,SENTBYTES=0,RECERRORS=0,SENTERRORS=0; //счетчики отправленных и принятых байт
QString fileName; //имя файла ПОСЛЕДНЕГО СОХРАНЕНИЯ, в который будем сохранять


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    mThread = new readThread();
    //Инициализируем рисунок
    plot = new QGraphicsScene (this);
    ui->graphicsView->setScene(plot);
    plot->setSceneRect(0,0,400,120);
    for (int i= 400; i>=0; i--)
    {
        plot->addLine(i,120,i,120);
        LINEBUF[i]=0;
    }
    //Здесь осуществляется захват данных с буфера устройства в параллельном потоке.
    //
    //Сигнал о готовности данных с буфера вывода в помещение их в основной текстбокс вывода комманд
    connect (mThread,SIGNAL(readyToWrite(QString)),ui->responseTextEdit,SLOT(appendPlainText(QString)));
    //считанные данные отправляем в процедуру парсинга.
    connect(mThread,SIGNAL(readyToWrite(QString)),this,SLOT(commandProccesing(QString)));
    //количество считанных данных отправляем в текстбокс статистики
    connect (mThread,SIGNAL(howMuchBytes(QString)),ui->statsList,SLOT(append(QString)));
    connect(mThread,SIGNAL(readyToWrite(QString)),this,SLOT(recievedBytesCounter(QString)));
}

MainWindow::~MainWindow()
{

    delete ui;
}

//в этой функции осуществляется соединение/отсоединение с портом, вывод ошибок, если есть,
//включение, отключение элементов управления
void MainWindow::on_connectButton_clicked()
{
   if (ui->connectButton->text()=="Connect")
   {
       //connect_disconnect=false;
       ui->connectButton->setText("Disconnect");
        PORTNAME = "/dev/ttyUSB"+QString::number(ui->portName->value());
        CONNECT_RESULT = connect_port(PORTNAME);
        switch(CONNECT_RESULT)
        {
        case 1: //error opening file descriptor
            ui->statsList->append("Error: cannot open " + PORTNAME);
            ui->statsList->append("(please check physical connection or permissions on " + PORTNAME + ")");
            ui->connectButton->setText("Connect");
            break;
        case 2://error opening control structure
            ui->statsList->append("Error: cannot get the parameters associated with the terminal (" + PORTNAME + ")");
            ui->connectButton->setText("Connect");
            break;
        case 3://error writing control structure to the port
            ui->statsList->append("Error: cannot get the parameters associated with the terminal (" + PORTNAME + ")");
            ui->connectButton->setText("Connect");
            break;
        case 0:
            {
            ui->statsList->append("Connected to " + PORTNAME);
            ui->frame->setEnabled(true);
            ui->frameDev->setEnabled(true);
            ui->responseTextEdit->setEnabled(true);
            mThread->start();
        }
        break;
    default:
        ui->responseTextEdit->appendPlainText("Error: unknown error " + PORTNAME);
        break;
    };
   } else
   {
       //connect_disconnect=true;
       ui->connectButton->setText("Connect");
       ::close(TTYUSB_PORT);
       ui->statsList->append("Port successfully closed");
       ui->frame->setEnabled(false);
       ui->frameDev->setEnabled(false);
       ui->responseTextEdit->setEnabled(false);
       mThread->exit(0);
   }
}

//ЗАПОМНИ! НИКОГДА И НИ ЗА ЧТО НЕ ВВОДИ В КОМАНДУ РУССКИЕ БУКВЫ!
//лень делать проверку на это, будем думать, что пользователь не дурак.

void MainWindow::on_sendButton_clicked()
{
    if (ui->onTimer->checkState()==false)
    {
        QCOMMAND =  ui->plainTextEdit->toPlainText();
        if (QCOMMAND!="")
        {
            sendCommand();
        }
    } else
    {
        if (ui->sendButton->text()=="Send")
        {
            QCOMMAND =  ui->plainTextEdit->toPlainText();
            if (QCOMMAND!="")
            {
                ui->sendButton->setText("Stop");
                MainWindow::timer = new QTimer(this);
                connect(MainWindow::timer,SIGNAL(timeout()),this,SLOT(sendCommand()));
                timer->start(ui->milliseconds->value());
            }
        } else
        {
            ui->sendButton->setText("Send");
            timer->stop();
        }
    }
}

/*Разделяем введенную команду в qCommandList, используя в качестве разделителя ";"
 * запускаем таймер, который будет вводить задержку между этими командами
 * qCommandIndex = 0; это глобальная переменная, нужна для остановки таймера,
 * когда она будет равна qCommandList.size()
 * ВНИМАНИЕ: данной функцией, как и _sendString()
 * пользоваться только из кнопки "SEND", иначе редиска.
 * */
void MainWindow::sendCommand()
{
    qCommandList = QCOMMAND.split(';', QString::SkipEmptyParts);
    ui->plainTextEdit->clear();
    qCommandIndex = 0; //qCommandList.size() - 1;
    delay = new QTimer(this);
    connect(delay,SIGNAL(timeout()),this,SLOT(_sendString()));
    delay->start(ui->delayCommands->value());
}

/*Здесь мы проверяем, отпарвилось или нет, затем останавливаем таймер задержки, когда нужно
 * а именно когда все команды из qCommandList уже отправлены
 * */
void MainWindow::_sendString()
{
    if (sendString(qCommandList[qCommandIndex].toLocal8Bit())!=0)
    {
        ui->statsList->append("Error Sending");
        SENTERRORS+=qCommandList[qCommandIndex].size()+1;
        ui->labelSentErrors->setText("Sent with errors: "+QString::number(SENTERRORS));
    } else
    {
        SENTBYTES+=qCommandList[qCommandIndex].size()+1;
        ui->labelSended->setText("Sent bytes: "+QString::number(SENTBYTES));
    }
    qCommandIndex++;
    if (qCommandIndex==qCommandList.size())
        delay->stop();
}

/* Обработка пришедших с порта результатов
 * В частности, здесь реализован парсер, если приходят показания с датчка температуры и света
 * функция засовывает значения в соответсвующие текстбоксы
 * РЕАЛИЗОВАТЬ: собственно, с какого устройства пришли данные
 * */
void MainWindow::commandProccesing(QString a)
{   //SREAD:7DFE,000D6F00024CBB8D,1F,00=FFEE так выглядит вывод с сенсоров

    //если пришло с температурного сенсора (поясняющие комментарии в следующем ИФ-е, лень дублировать)
    if ((a.contains("SREAD"))&&(a.contains(",1F,")))
    {
        QString temp;
        int pos = a.indexOf(",1F,",0)+7;
        for (int i = pos; i< pos+4; i++)
        {
            temp[i-pos] = a[i];
        }
        ui->tempEdit->setText(QString::number(temp.toInt(0,16)/400.0)+"°C");
        plotValue((temp.toInt(0,16))*120/20000);
        plot->addText("Temperature value: "+QString::number(temp.toInt(0,16)/400.0)+"°C");
        if (ui->saveSensData->isChecked())
        {
            if (FILE_OUT.is_open())
            {
                FILE_OUT << ui->milliseconds->value()*INDEX_OF_WRITE << " " << temp.toInt(0,16)/400.0<< "\n";
                INDEX_OF_WRITE++;
            }
        }
    }

    //если со светового сенсора
    if ((a.contains("SREAD"))&&(a.contains(",22,")))
    {
        QString light; //значение с датчика света, которое мы отпарсим с вывода устройства
                       //имеет формат FFFF, двухбайтовое число 0<<65536
        int pos = a.indexOf(",22,",0)+7;
        for (int i = pos; i< pos+4; i++)
        {
            light[i-pos] = a[i];
        } //отпарсили, все хорошо
        ui->lightEdit->setText(QString::number(light.toInt(0,16)));
        plotValue((light.toInt(0,16))*120/30000); //делим не на 65536, а на меньшее число, так как значения как правило там
                                                  //редко выходят за границы диапазона 0<<20000
        plot->addText("Light value: "+QString::number(light.toInt(0,16)));
        if (ui->saveSensData->isChecked())
        {
            if (FILE_OUT.is_open())
            {
                FILE_OUT << ui->milliseconds->value()*INDEX_OF_WRITE << " " << light.toInt(0,16)<< "\n";
                INDEX_OF_WRITE++;
            }
        }
    }
    /*Реализовать: считывание номеров (длинных и коротких) и помещение их в текстбокс устройств
     * после команды at+sn
     * */
}

/*Рисование графика показателей с сенсоров.
 * принимает интовое значение только что считанной величины.
 * При инициализации поля рисунка я подвинул точку (0;0) в верхний левый угол.
 * Следовательно нижний правый угол будет иметь координаты (400;120) -- из геометрических размеров самого поля
 * (Смотри в свойствах объекта gaphicsView)
 * график представляет собой набор линий пиксельной толщины, расположенных вертикально, забором, по всей нижней границе поля.
 * длина этих самых линий будет равна нашей величине.
 * то есть отрисовка нового значения поля справа означает удаление самого левого значения,
 * сдвиг всех четырехсот значений влево на один, и самое правое значение будет величина, данная в аргументе.
 * Для этого и нужен массив значений LINEBUF, каждый раз мы просто заново отрисовываем график.
 * */
void MainWindow::plotValue(int d)
{
    for (int i = 0; i<399; i++)
    {
        LINEBUF[i]=LINEBUF[i+1];
    }
    LINEBUF[399]=d;
    plot->clear();
    for (int i= 400; i>=0; i--)
    {
        plot->addLine(i,120,i,120-LINEBUF[i],PEN);
    }
}

//посылаем данные на плату о включении татчика температуры,
//затем включаем ставим на таймер и считываем показатели.
void MainWindow::on_tempSensor_clicked()
{
    if (ui->devNumber->text()!="")
    {
        if (ui->tempSensor->text()=="Temperature sensor")
        {
            if (ui->saveSensData->isChecked())
            {
                FILE_OUT.open("TEMPERATURE_SENSOR_DATA");
                FILE_OUT << "TIME(msec.)  VALUE(°C)\n";
                INDEX_OF_WRITE=0;
            }

            PEN.setColor(QColor(0,0,255));
            ui->tempSensor->setText("Stop");
            QString settings;
            ui->lightSensor->setEnabled(false);
            settings = "atrems:"+ui->devNumber->text()+",15D=1";

            if (sendString(settings.toLocal8Bit())!=0)
            {
                ui->statsList->append("Error Sending");
                SENTERRORS+=settings.size()+1;
                ui->labelSentErrors->setText("Sent with errors: "+QString::number(SENTERRORS));
            } else
            {
                SENTBYTES+=settings.size()+1;
                ui->labelSended->setText("Sent bytes: "+QString::number(SENTBYTES));
            }

            settings = "atrems:"+ui->devNumber->text()+",182=1";
            if (sendString(settings.toLocal8Bit())!=0)
            {
                ui->statsList->append("Error Sending");
                SENTERRORS+=settings.size()+1;
                ui->labelSentErrors->setText("Sent with errors: "+QString::number(SENTERRORS));
            } else
            {
                SENTBYTES+=settings.size()+1;
                ui->labelSended->setText("Sent bytes: "+QString::number(SENTBYTES));
            }

            MainWindow::timer = new QTimer(this);
            connect(MainWindow::timer,SIGNAL(timeout()),this,SLOT(requestTemp()));
            timer->start(ui->milliseconds->value());
        } else
        {
            ui->lightSensor->setEnabled(true);
            ui->tempSensor->setText("Temperature sensor");
            timer->stop();
            if (FILE_OUT.is_open()) FILE_OUT.close();
            INDEX_OF_WRITE=0;
        }
    } else
    {
        ui->devNumber->setText("?");
    }
}

//запрашивает у устройства показание с датчика температуры.
//для устройства, указанного в текстбоксе devNumber
void MainWindow::requestTemp()
{
    QString device = "atrems:"+ui->devNumber->text()+",1f?";
    if (sendString(device.toLocal8Bit())!=0)
    {
        ui->statsList->append("Error Sending");
        SENTERRORS+=device.size()+1;
        ui->labelSentErrors->setText("Sent with errors: "+QString::number(SENTERRORS));
    } else
    {
        SENTBYTES+=device.size()+1;
        ui->labelSended->setText("Sent bytes: "+QString::number(SENTBYTES));
    }
}

//посылаем данные на плату о включении татчика освещенности,
//затем включаем ставим на таймер и считываем показатели.
void MainWindow::on_lightSensor_clicked()
{
    if (ui->devNumber->text()!="")
    {
        if (ui->lightSensor->text()=="Light sensor")
        {
            if (ui->saveSensData->isChecked())
            {
                FILE_OUT.open("LIGHT_SENSOR_DATA");
                FILE_OUT << "TIME(msec.)  VALUE\n";
                INDEX_OF_WRITE=0;
            }
            ui->tempSensor->setEnabled(false);
            PEN.setColor(QColor(255,0,0));
            ui->lightSensor->setText("Stop");
            QString settings;
            settings = "atrems:"+ui->devNumber->text()+",1511=1";

            if (sendString(settings.toLocal8Bit())!=0)
            {
                ui->statsList->append("Error Sending");
                SENTERRORS+=settings.size()+1;
                ui->labelSentErrors->setText("Sent with errors: "+QString::number(SENTERRORS));
            }else
            {
                SENTBYTES+=settings.size()+1;
                ui->labelSended->setText("Sent bytes: "+QString::number(SENTBYTES));
            }
            settings = "atrems:"+ui->devNumber->text()+",183=1";
            if (sendString(settings.toLocal8Bit())!=0)
            {
                ui->statsList->append("Error Sending");
                SENTERRORS+=settings.size()+1;
                ui->labelSentErrors->setText("Sent with errors: "+QString::number(SENTERRORS));
            }else
            {
                SENTBYTES+=settings.size()+1;
                ui->labelSended->setText("Sent bytes: "+QString::number(SENTBYTES));
            }

            MainWindow::timer = new QTimer(this);
            connect(MainWindow::timer,SIGNAL(timeout()),this,SLOT(requestLight()));
            timer->start(ui->milliseconds->value());
        } else
        {
            ui->tempSensor->setEnabled(true);
            ui->lightSensor->setText("Light sensor");
            timer->stop();
            if (FILE_OUT.is_open()) FILE_OUT.close();
            INDEX_OF_WRITE=0;
        }
    } else
    {
        ui->devNumber->setText("?");
    }
}

//запрашивает у устройства показание с датчика освещенности
//для устройства, указанного в текстбоксе devNumber
void MainWindow::requestLight()
{
    QString device = "atrems:"+ui->devNumber->text()+",22?";
    if (sendString(device.toLocal8Bit())!=0)
    {
        ui->statsList->append("Error Sending");
        SENTERRORS+=device.size()+1;
        ui->labelSentErrors->setText("Sent with errors: "+QString::number(SENTERRORS));
    }else
    {
        SENTBYTES+=device.size()+1;
        ui->labelSended->setText("Sent bytes: "+QString::number(SENTBYTES));
    }
}

/* Четыре кнопки для включения четырех лампочек
*/

void MainWindow::on_LED1_clicked()
{
    /*QString device = "atrems:"+ui->devNumber->text()+",18=00000040";
    if (sendString(device.toLocal8Bit())!=0)
    {
        ui->responseTextEdit->appendPlainText("Error Sending");
    }*/
    if (ui->LED1->isChecked())
    {

    }else
    {

    }
}

void MainWindow::on_LED2_clicked()
{
    /*QString device = "atrems:"+ui->devNumber->text()+",18=00000080";
    if (sendString(device.toLocal8Bit())!=0)
    {
        ui->responseTextEdit->appendPlainText("Error Sending");
    }*/
    if (ui->LED2->isChecked())
    {

    }else
    {

    }
}

void MainWindow::on_LED3_clicked()
{
    /*QString device = "atrems:"+ui->devNumber->text()+",18=00004000";
    if (sendString(device.toLocal8Bit())!=0)
    {
        ui->responseTextEdit->appendPlainText("Error Sending");
    }*/
    if (ui->LED3->isChecked())
    {

    }else
    {

    }
}

void MainWindow::on_LED4_clicked()
{
    /*QString device = "atrems:"+ui->devNumber->text()+",18=00010000";
    if (sendString(device.toLocal8Bit())!=0)
    {
        ui->responseTextEdit->appendPlainText("Error Sending");
    }*/
    if (ui->LED4->isChecked())
    {

    }else
    {

    }
}

//-----------Здесь методы для окна статистики----------
/*Очистить окно статистики*/
void MainWindow::on_clearStats_clicked()
{
    ui->labelRecieved->setText("Received bytes: 0");
    ui->labelSended->setText("Sent bytes: 0");
    ui->labelRecErrors->setText("Recieved with errors: 0");
    ui->labelSentErrors->setText("Sent with errors: 0");
    SENTBYTES =0;
    RECIEVED_BYTES=0;
}
/*слот, принимающий строку, которую считали, чтобы преобразовать её в количество байт
и вывести в окно статистики*/
void MainWindow::recievedBytesCounter(QString str)
{
    RECIEVED_BYTES+=str.size();
    ui->labelRecieved->setText("Received bytes: "+QString::number(RECIEVED_BYTES));
}

//---------после этой черты пиши методы потока чтения---------------

void readThread::run()
{
    QString a;
    char *c = new char[1000];
    int bytes=0;
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(TTYUSB_PORT, &rfds);
    while(1)
    {
        if (select(TTYUSB_PORT+1,&rfds,NULL,NULL,NULL)==1)
        {
            ioctl(TTYUSB_PORT, FIONREAD, &bytes);
            switch (bytes)
            {
            case 0:
            {
                emit howMuchBytes("0 bytes were read");
                break;
            }
            case -1:
            {
                emit howMuchBytes("Error: something wrong");
                break;
            }
            default:
                {
                    read(TTYUSB_PORT,c,bytes);
                    c[bytes]='\0';
                    a=QString::fromStdString(c);
                    emit readyToWrite(a);
                    emit howMuchBytes(QString::number(bytes)+" bytes were read");
                    break;
                }
            }
        }
    }
    delete[]c;
}

void MainWindow::on_pushButton_clicked()
{
    QMessageBox::information(this,"LANA-information","LANA - terminal for Telegesis® Devices\n\n"
                             "System specifications:\n1"
                             "9200bps, Data bits - 8, Parity - none, Stop bits - 1,\n"
                             "Flow Control – none\n\n"
                             "Developers: Leonid Kruglov, Artem Yushkovsky,\n"
                             "Nikita Druzhinin, Alexey Ilyin\n\n"
                             "Supervisors: Ilya Lebedev, Viktoria Korzhuk");
}



//---------SAVE TO FILE----------------------------------------------------------------------------------------------------------------------
void MainWindow::saveFilename(QString plainText)
{// создае файл с непустым именем fileName и пишет в него:
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly))//QIODevice::Truncate ||
    {
        QMessageBox::information(this, tr("Unable to open file"),
            file.errorString());
        return;
    }
    QDataStream out(&file);
    out.setVersion(QDataStream::Qt_4_5);
    out << plainText;
    file.close();
}

void MainWindow::on_pushButton_5_clicked() // Save As...
{
    ui->plainTextEdit->setEnabled(true);
    ui->plainTextEdit->appendPlainText("12345, dsitk pfqxbr gjuekznm!!");

    QString plainText = ui->plainTextEdit->toPlainText();

    if (!plainText.isEmpty())
    {
        fileName = QFileDialog::getSaveFileName(this,
                 tr("Save Command History"), "Untiled",
                 tr( (fileName+tr("(*.txt);;All Files (*)")).toUtf8() ));
        fileName = fileName.split("/")[fileName.count('/')];
        if (fileName == tr(".txt")) fileName = tr("Untiled_dump.txt");
        if (! (fileName.endsWith(".txt", Qt::CaseInsensitive)))
                fileName += ".txt"; // default
        else saveFilename(plainText);
        QMessageBox::information(this, "Success", "Your data saved to "+fileName);
    }
    else
        QMessageBox::information(this,"Error", "Sorry, the command history is empty. Please, connect to the device and send some commands.");
    if(!ui->pushButton_2->isEnabled()) ui->pushButton_2->setEnabled(true);

    if (fileName.length()>8+4) ui->pushButton_2->setText(tr("Save to ")+fileName.left(8)+"..."+".txt");
    else ui->pushButton_2->setText(tr("Save to ")+fileName);
}

void MainWindow::on_pushButton_2_clicked() // Save ...
{// ПОКА функция выше "saveFilename()" ТОЛЬКО ДОБАВЛЯЕТ В КОНЕЦ,
    //хотя из-за режима QIODevice::WriteOnly должна переписывать.
    // Может, писать не потоком, а через fopen() ?

    ui->plainTextEdit->setEnabled(true);
    ui->plainTextEdit->appendPlainText("new filesave");

    QString plainText = ui->plainTextEdit->toPlainText();
    if (!plainText.isEmpty())
    {
        if (fileName.isEmpty())
        {
            QMessageBox::information(this, "Error", tr("The file to save in is not selected. Please, click 'Save As' button"));
            return;
        }
        else saveFilename(plainText);
    }
    else
        QMessageBox::information(this,"Error", "Sorry, the command history is empty. Please, connect to the device and send some commands.");
}//--------------------------------------------------------------------------------------------------------------------------------------------
