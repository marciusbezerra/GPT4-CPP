#include "mainwindow.h"
#include "styleditemdelegate.h"
#include "ui_mainwindow.h"

#include <QStandardItemModel>
#include <QDateTime>
#include <QMessageBox>

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextDocument>
#include <QFile>
#include <QThread>
#include <QDesktopServices>
#include <QSettings>
#include <QCloseEvent>
#include <QFileDialog>
#include <QBuffer>
#include "version.h"

// Informações de versão foram postas em version.h e resources.rc
// (poderiam estar apenas no .pro também, RC_ICONS e VERSION)

// for Windows deployment
// mudar para a compilação para release
// compilar o projeto
// limpar o diretório release deixando apenas o exe
// abrir o prompt do Qt msvc2019_64
// cd ../release
// windeployqt .

// for mac deployment
// mudar para a compilação para release
// compilar o projeto
// abrir o terminal
// 6.6.2/macos/bin/macdeployqt /Users/marciusbezerra/Projetos/c++/build-qt-gpt4-Qt_6_6_2_for_macOS-Release/qt-gpt4.app -dmg
// open /Users/marciusbezerra/Projetos/c++/build-qt-gpt4-Qt_6_6_2_for_macOS-Release/
// executar o dmg
// copiar para a pasta de aplicativos

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // Atualizar a versão também em VERSION (arquivo .pro)
    QCoreApplication::setApplicationVersion(VER_PRODUCTVERSION_STR);
    setWindowTitle("GPT4 - Turbo (MCBMax.com) v" + QCoreApplication::applicationVersion());

    fillModelList();

    loadConversations("conversations.json");
    fillConversationTreeView();

    QSettings settings("OpenAI", "GPT");

    apiKey = settings.value("api_key").toString();

    ui->comboBoxModel->setItemDelegate(new StyledItemDelegate(ui->comboBoxModel));

    gptModel = settings.value("model").toString();
    if (gptModel.isEmpty()) {
        gptModel = "gpt-4o";
    }

    ui->lineEditApiKey->setText(apiKey);
    setComboBoxToId(ui->comboBoxModel, gptModel);

    connect(ui->treeViewChats->model(), &QAbstractItemModel::dataChanged, this, &MainWindow::onItemChanged);
    ui->treeViewChats->setContextMenuPolicy(Qt::CustomContextMenu);

    ui->labelImage->installEventFilter(this);
    ui->labelImage->setCursor(Qt::PointingHandCursor);

    connect(ui->treeViewChats, &QTreeView::clicked, this, &MainWindow::onTreeViewChatsClicked);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setComboBoxToId(QComboBox *comboBox, QString id) {
    for (int i = 0; i < comboBox->count(); ++i) {
        if (comboBox->itemData(i).toString() == id) {
            comboBox->setCurrentIndex(i);
            return;
        }
    }
    qDebug() << "ID not found in comboBox";
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->labelImage && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) {
            on_labelImage_clicked();
            return true;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::on_labelImage_clicked()
{
    qDebug() << "on_labelImage_clicked";
    QMenu contextMenu(this);
    QAction actionClear("Apagar", this);
    QAction actionLoad("Carregar", this);
    connect(&actionClear, &QAction::triggered, this, [this]() {
        resetImage();
    });
    connect(&actionLoad, &QAction::triggered, this, [this]() {
        QString fileName = QFileDialog::getOpenFileName(this, "Carregar Imagem", "", "Imagens (*.png *.jpg *.jpeg)");
        if (!fileName.isEmpty()) {
            ui->labelImage->clear();
            QPixmap pixmap(fileName);
            if (pixmap.width() > 512 || pixmap.height() > 512) {
                pixmap = pixmap.scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            ui->labelImage->setPixmap(pixmap);
            ui->labelImage->setScaledContents(true);
        }
    });
    contextMenu.addAction(&actionClear);
    contextMenu.addAction(&actionLoad);
    contextMenu.exec(ui->labelImage->mapToGlobal(QPoint(15, 15)));
}

void MainWindow::fillModelList()
{
    ui->comboBoxModel->clear();

    ui->comboBoxModel->addItem("<b>gpt-4o</b><br><i>Com Vision, mais avançado, rápido, multilíngue e barato - 128.000 tokens até 2023</i>", QVariant("gpt-4o"));
    ui->comboBoxModel->addItem("<b>gpt-4o-mini</b><br><i>Com Vision, melhor e mais barato que o GPT-3.5 Turbo, rápido, multilíngue e barato - 128.000 tokens até 2023</i>", QVariant("gpt-4o-mini"));
    ui->comboBoxModel->addItem("<b>gpt-4-turbo</b><br><i>GPT-4 Turbo com Vision - 128.000 tokens até 2023</i>", QVariant("gpt-4-turbo"));
    ui->comboBoxModel->addItem("<b>gpt-3.5-turbo-0125</b><br><i>16.385 tokens até 2021</i>", QVariant("gpt-3.5-turbo-0125"));
    ui->comboBoxModel->addItem("<b>gpt-3.5-turbo</b><br><i>16.385 tokens até 2021</i>", QVariant("gpt-3.5-turbo"));
    ui->comboBoxModel->addItem("<b>gpt-3.5-turbo-instruct</b><br><i>4.096 tokens até 2021</i>", QVariant("gpt-3.5-turbo-instruct"));
}

void MainWindow::fillConversationTreeView() {

    QStandardItemModel *model = dynamic_cast<QStandardItemModel*>(ui->treeViewChats->model());
    if (!model) {
        model = new QStandardItemModel(this);
        ui->treeViewChats->setModel(model);
    }

    model->clear();

    QStandardItem *lastHistoryDate = nullptr;
    QStandardItem *lastConversation = nullptr;

    for (const auto &historyDate : historyDateList) {
        lastHistoryDate = new QStandardItem(QIcon(":/new/images/resources/images/send.png"), historyDate->date);
        lastHistoryDate->setFlags(lastHistoryDate->flags() & ~Qt::ItemIsEditable);
        model->appendRow(lastHistoryDate);

        for (const auto &conversation : historyDate->conversationList) {
            QStandardItem *conversationItem = new QStandardItem(QIcon(":/new/images/resources/images/request.png"), conversation->title);
            conversationItem->setData(conversation->id, Qt::UserRole);
            lastHistoryDate->appendRow(conversationItem);
            lastConversation = conversationItem;
        }
    }

    if (lastHistoryDate && lastConversation) {
        ui->treeViewChats->expand(lastHistoryDate->index());
        ui->treeViewChats->setCurrentIndex(lastConversation->index());
        auto conversation = locateConversation(lastConversation);
        if (conversation) {
            fillChatListWidget(conversation);
        }
    }
}

std::shared_ptr<Conversation> MainWindow::locateConversation(QStandardItem *item) {
    auto conversationId = item->data(Qt::UserRole).toString();
    for (const auto &historyDate : historyDateList) {
        for (const auto &conversation : historyDate->conversationList) {
            if (conversation->id == conversationId) {
                return conversation;
            }
        }
    }
    return nullptr;
}

void MainWindow::on_toolButtonSendQuestion_clicked()
{
    apiKey = ui->lineEditApiKey->text();
    auto question = ui->textEditQuestion->toPlainText();

    if(apiKey.isEmpty()) {
        QMessageBox::warning(this, "API Key não informada", "Informe a API Key para continuar");
        statusBar()->showMessage("API Key não informada", 3000);
        return;
    }

    if(question.isEmpty()) {
        QMessageBox::warning(this, "Pergunta não informada", "Digite uma pergunta para continuar");
        statusBar()->showMessage("Digite uma pergunta", 3000);
        return;
    }

    lockUi(true);
    showProgressDialog("Aguarde, consultado a API OpenAI...");

    auto QPixmapToBase64 = [](const QPixmap &pixmap) {
        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        buffer.open(QIODevice::WriteOnly);
        pixmap.save(&buffer, "PNG");
        return byteArray.toBase64();
    };

    lastQuestionAnswer = std::make_shared<QuestionAnswer>();
    lastQuestionAnswer->question = question;
    lastQuestionAnswer->answer = "";
    if (!ui->labelImage->pixmap().isNull()) {
        lastQuestionAnswer->image = "data:image/png;base64," + QPixmapToBase64(ui->labelImage->pixmap());
    } else {
        lastQuestionAnswer->image = "";
    }

    createTodayConversationIfNotExists();
    currentConversation->questionAnswerList.append(lastQuestionAnswer);

    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    connect(manager, &QNetworkAccessManager::finished, this, &MainWindow::on_networkRequestFinished);

    QNetworkRequest request(QUrl("https://api.openai.com/v1/chat/completions"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());

    QJsonObject body;
    QJsonArray messagesArray;

    if (lastQuestionAnswer->image.isEmpty()) {
        for (const auto &questionAnswer : currentConversation->questionAnswerList) {
            QJsonObject chatObject;
            chatObject["role"] = "user";
            chatObject["content"] = questionAnswer->question;
            if (!questionAnswer->answer.isEmpty()) {
                chatObject["role"] = "assistant";
                chatObject["content"] = questionAnswer->answer;
            }
            messagesArray.append(chatObject);
        }
    } else {
        QJsonObject chatObject;
        chatObject["role"] = "user";
        chatObject["content"] = QJsonArray({
            QJsonObject({
                {"type", "text"},
                {"text", lastQuestionAnswer->question }
            }),
            QJsonObject({
                {"type", "image_url"},
                {"image_url", QJsonObject({
                                  {"url", lastQuestionAnswer->image},
                                  // {"detail", "high"}
                              })}
            })
        });
        messagesArray.append(chatObject);
    }

    body["messages"] = messagesArray;
    body["model"] = ui->comboBoxModel->currentData().toString();
    body["temperature"] = 0.5;
    body["max_tokens"] = 4000;

    QByteArray bodyJson = QJsonDocument(body).toJson();

    // gravar bodyJson em um arquivo...
    QFile file("debug.json");
    file.open(QIODevice::WriteOnly);
    file.write(bodyJson);
    file.close();

    manager->post(request, bodyJson);
}

void MainWindow::on_networkRequestFinished(QNetworkReply *reply)
{
    if(reply->error() != QNetworkReply::NoError) {
        hideProgressDialog();
        lockUi(false);
        auto error = reply->readAll();
        qDebug() << error;
        if (error.startsWith("{")) {
            QJsonDocument docJson = QJsonDocument::fromJson(error);
            QJsonObject obj = docJson.object();
            QString message = obj["error"].toObject()["message"].toString();
            QMessageBox::warning(this, "Erro na requisição", message);
            return;
        }
        QMessageBox::warning(this, "Erro na requisição", "Erro ao enviar a pergunta: " + reply->errorString());
        return;
    }

    QJsonDocument docJson = QJsonDocument::fromJson(reply->readAll());
    QJsonObject obj = docJson.object();
    QString answer = obj["choices"].toArray().first().toObject()["message"].toObject()["content"].toString();

    lastQuestionAnswer->answer = answer;

    fillChatListWidget(currentConversation);
    saveConversations("conversations.json");

    hideProgressDialog();
    lockUi(false);
    ui->textEditQuestion->clear();
    resetImage();
}

void MainWindow::resetImage() {
    ui->labelImage->clear();
    ui->labelImage->setText("Clique para carregar uma imagem");
}

void MainWindow::showProgressDialog(const QString &text)
{
    if (!progressDialog) {
        progressDialog = new QProgressDialog(text, "Cancelar", 0, 0, this);
        progressDialog->setWindowTitle(this->windowTitle());
        progressDialog->setWindowModality(Qt::WindowModal);
    } else {
        progressDialog->setLabelText(text);
    }
    progressDialog->show();
}

void MainWindow::hideProgressDialog()
{
    if (progressDialog) {
        progressDialog->hide();
    }
}

void MainWindow::lockUi(bool lock)
{
    ui->lineEditApiKey->setEnabled(!lock);
    ui->treeViewChats->setEnabled(!lock);
    ui->groupBoxQuestion->setEnabled(!lock);
    // ui->textEditQuestion->setEnabled(!lock);
    // ui->pushButtonSendQuestion->setEnabled(!lock);
}

void MainWindow::createTodayConversationIfNotExists() {

    if (!currentConversation) {

        std::shared_ptr<HistoryDate> historyDateForToday = nullptr;
        QString today = QDateTime::currentDateTime().toString("dd/MM/yyyy");

        // se não existe nenhuma conversa, para a data de hoje...
        for (const auto &historyDate : historyDateList) {
            if (historyDate->date == today) {
                historyDateForToday = historyDate;
                break;
            }
        }

        // se não existe nenhuma conversa para a data de hoje, cria uma nova
        if (!historyDateForToday) {
            historyDateForToday = std::make_shared<HistoryDate>();
            historyDateForToday->date = today;
            historyDateForToday->conversationList = QList<std::shared_ptr<Conversation>>();
            historyDateList.append(historyDateForToday);
        }

        currentConversation = std::make_shared<Conversation>();
        currentConversation->id = QUuid::createUuid().toString();
        currentConversation->title = "Conversa #" + QString::number(historyDateForToday->conversationList.size() + 1);
        currentConversation->questionAnswerList = QList<std::shared_ptr<QuestionAnswer>>();
        historyDateForToday->conversationList.append(currentConversation);

        fillConversationTreeView();
    }

}

void MainWindow::loadConversations(const QString &filename)
{
    QString absolutePath = QCoreApplication::applicationDirPath() + "/" + filename;
    QFile file(absolutePath);
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("Não foi possível abrir o arquivo para leitura.");
        return;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc(QJsonDocument::fromJson(data));
    QJsonArray array = doc.array();

    historyDateList = QList<std::shared_ptr<HistoryDate>>();

    for (int i = 0; i < array.size(); ++i) {
        QJsonObject historyDateObject = array[i].toObject();
        auto historyDate = std::make_shared<HistoryDate>();
        historyDate->date = historyDateObject["date"].toString();

        QJsonArray conversationArray = historyDateObject["conversationList"].toArray();
        for (int j = 0; j < conversationArray.size(); ++j) {
            QJsonObject conversationObject = conversationArray[j].toObject();
            auto conversation = std::make_shared<Conversation>();
            conversation->id = conversationObject["id"].toString();
            conversation->title = conversationObject["title"].toString();

            QJsonArray chatsArray = conversationObject["questionAnswerList"].toArray();
            for (int k = 0; k < chatsArray.size(); ++k) {
                QJsonObject chatObject = chatsArray[k].toObject();
                auto questionAnswer = std::make_shared<QuestionAnswer>();
                questionAnswer->question = chatObject["question"].toString();
                questionAnswer->answer = chatObject["answer"].toString();
                questionAnswer->image = chatObject["image"].toString();
                conversation->questionAnswerList.append(questionAnswer);
            }

            historyDate->conversationList.append(conversation);
        }

        historyDateList.append(historyDate);
    }
}

void MainWindow::saveConversations(const QString &filename)
{   
    QString absolutePath = QCoreApplication::applicationDirPath() + "/" + filename;
    QFile file(absolutePath);

    if (!file.open(QIODevice::WriteOnly)) {
        qWarning("Não foi possível abrir o arquivo para escrita.");
        return;
    }

    QJsonArray array;
    for (const auto &historyDate : historyDateList) {
        QJsonObject historyDateObject;
        historyDateObject["date"] = historyDate->date;

        QJsonArray conversationArray;
        for (const auto &conversation : historyDate->conversationList) {
            QJsonObject conversationObject;
            conversationObject["id"] = conversation->id;
            conversationObject["title"] = conversation->title;

            QJsonArray chatsArray;
            for (const auto &questionAnswer : conversation->questionAnswerList) {
                QJsonObject chatObject;
                chatObject["question"] = questionAnswer->question;
                chatObject["answer"] = questionAnswer->answer;
                chatObject["image"] = questionAnswer->image;
                chatsArray.append(chatObject);
            }
            conversationObject["questionAnswerList"] = chatsArray;
            conversationArray.append(conversationObject);
        }
        historyDateObject["conversationList"] = conversationArray;
        array.append(historyDateObject);
    }

    QJsonDocument doc(array);
    file.write(doc.toJson());
}

void MainWindow::onTreeViewChatsClicked(const QModelIndex &index)
{
    if (index.parent().isValid()) {
        QString conversationId = index.data(Qt::UserRole).toString();
        for (const auto &historyDate : historyDateList) {
            for (const auto &conversation : historyDate->conversationList) {
                if (conversation->id == conversationId) {
                    currentConversation = conversation;
                    break;
                }
            }
        }
        if (currentConversation) {
            fillChatListWidget(currentConversation);
        }
    } else {
        currentConversation = nullptr;
        fillChatListWidget(nullptr);
    }
}

void MainWindow::fillChatListWidget(std::shared_ptr<Conversation> conversation)
{
    ui->textBrowser->clear();
    if (conversation) {
        currentConversation = conversation;
        QString document = QString("## %1\n\n---\n\n").arg(conversation->title);
        ui->textBrowser->append(document);

        for (const auto &questionAnswer : conversation->questionAnswerList) {
            if (!questionAnswer->image.isEmpty()) {
                document += QString("![image](%1)\n\n").arg(questionAnswer->image);
            }
            document += QString("❓ **Você:** *%1*\n\n").arg(questionAnswer->question);
            document += QString("💡 **GPT:** %1\n\n---\n\n").arg(questionAnswer->answer);
        }

        QTextDocument doc;
        doc.setMarkdown(document);
        auto documentHtml = doc.toHtml();
        ui->textBrowser->setHtml(documentHtml);
        ui->textBrowser->moveCursor(QTextCursor::End);
        ui->textBrowser->ensureCursorVisible();
    }
}

void MainWindow::on_treeViewChats_doubleClicked(const QModelIndex &index)
{
    qDebug() << "on_treeViewChats_doubleClicked";
    if (index.parent().isValid()) {
        ui->treeViewChats->edit(index);
    }
}

void MainWindow::on_treeViewChats_customContextMenuRequested(const QPoint &pos)
{
    if (currentConversation) {
        QMenu contextMenu(this);
        QAction action("Apagar", this);
        connect(&action, &QAction::triggered, this, &MainWindow::deleteChat);
        contextMenu.addAction(&action);
        contextMenu.exec(ui->treeViewChats->mapToGlobal(pos));
    }
}

void MainWindow::deleteChat()
{
    if (currentConversation) {
        if (QMessageBox::question(this, "Apagar conversa", "Tem certeza que deseja apagar esta conversa?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
            for (const auto &historyDate : historyDateList) {
                for (int i = 0; i < historyDate->conversationList.size(); ++i) {
                    if (historyDate->conversationList[i]->id == currentConversation->id) {
                        historyDate->conversationList.removeAt(i);
                        break;
                    }
                }
            }
            fillConversationTreeView();
            fillChatListWidget(nullptr);
            saveConversations("conversations.json");
        }
    }
}

void MainWindow::on_actionSair_triggered()
{
    close();
}


void MainWindow::on_commandLinkButtonNewChat_clicked()
{
    currentConversation = nullptr;
    fillChatListWidget(currentConversation);
}

void MainWindow::onItemChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    Q_UNUSED(bottomRight);
    Q_UNUSED(roles);

    if (topLeft.isValid()) {
        auto item = dynamic_cast<QStandardItemModel*>(ui->treeViewChats->model())->itemFromIndex(topLeft);

        if (item) {
            auto conversation = locateConversation(item);

            if (conversation) {
                if (item->text().isEmpty()) {
                    item->setText(conversation->title);
                } else {
                    conversation->title = item->text();
                    saveConversations("conversations.json");
                }
            }
        }
    }
}


void MainWindow::on_actionCreditos_triggered()
{
    QMessageBox::about(this,
                       "Créditos",
                       "Desenvolvido por:<br><b>MARCIUS C. BEZERRA</b><br><br>"
                       "Linguagem: <b>C++</b><br>"
                       "Contato: <b><a href='tel:+5585988559171'>85 98855-9171</a></b><br>"
                       "E-mail: <b><a href='mailto:marciusbezerra@gmail.com'>marciusbezerra@gmail.com</a></b><br>");
}


void MainWindow::on_actionCriar_API_Key_triggered()
{
    QMessageBox msgBox;
    msgBox.setWindowTitle("Criar API Key");
    msgBox.setText("Passos para criar uma API Key na OpenAI:<br><br>"
                   "1. Crie uma conta OpenAI:<br>"
                   "<b> Você pode usa sua conta Google!</b><br>"
                   "2. Acesse a página de API Keys:<br>"
                   "<a href='https://platform.openai.com/api-keys'>https://platform.openai.com/api-keys</a><br>"
                   "3. Crie e copie uma nova API Key<br>"
                   "4. Cole no campo API Key desse aplicativo<br><br>");
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.addButton(QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);
    if(msgBox.exec() == QMessageBox::Yes){
        QDesktopServices::openUrl(QUrl("https://platform.openai.com/api-keys"));
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (QMessageBox::question(this, "Sair", "Deseja realmente sair?") == QMessageBox::Yes) {

        saveConversations("conversations.json");
        QSettings settings("OpenAI", "GPT");
        settings.setValue("api_key", ui->lineEditApiKey->text());
        settings.setValue("model", ui->comboBoxModel->currentData().toString());

        event->accept();
    } else {
        event->ignore();
    }
}

