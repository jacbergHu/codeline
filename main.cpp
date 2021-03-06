#include <QCoreApplication>
#include <QDebug>
#include <QString>
#include <QStringList>
#include <QFileInfoList>
#include <QFileInfo>
#include <QDir>
#include "pthread.h"

#define VERSION "codeline V1.0"
#define THREADCOUNT 10
struct FileCountResult {
    QString     fileName;
    QString     path;
    int totalLines   = 0;
    int codeLines    = 0;
    int commentLines = 0;
    int blankLines   = 0;
};

struct ThreadParams {
    QFileInfoList list;
};

bool isDebugfileName = false;
QFileInfoList allFileList;
QString     filterFileType;
QString     singleLineComment = QString("//");
QString     multiLineCommentsBegin = QString("/*");
QString     multiLineCommentsEnd = QString("*/");

struct FileCountResult allFileResult;

pthread_mutex_t mutex;

void* ProcessData(void* para) {
    ThreadParams* param = (ThreadParams*)para;
    QList<FileCountResult> fileResultList;
    foreach(QFileInfo fileInfo, param->list) {
        FileCountResult result;
        QFile file(fileInfo.absoluteFilePath());
        if(!file.open(QIODevice::Text | QIODevice::ReadOnly)) {
            qDebug() << "open file failed" << fileInfo.absoluteFilePath();
            break;
        }
        while(!file.atEnd()) {
            QString line = file.readLine().trimmed();
            if(line.isEmpty()) {
                result.blankLines += 1;
            } else if(line.startsWith(singleLineComment)) {
                result.commentLines += 1;
            } else {
                int posmultiLineCommentsBegin = line.indexOf(multiLineCommentsBegin);
                if(posmultiLineCommentsBegin != 0) {
                    result.codeLines += 1;
                } else if(posmultiLineCommentsBegin == 0) {
                    result.commentLines += 1;
                }

                if(posmultiLineCommentsBegin != -1) {
                    if(!line.contains(multiLineCommentsEnd)) {
                        while(!file.atEnd()) {
                            QString nextLine = file.readLine().trimmed();
                            result.commentLines += 1;
                            if(nextLine.contains(multiLineCommentsEnd)) {
                                if(!nextLine.endsWith(multiLineCommentsEnd)) {
                                    result.codeLines += 1;
                                    result.commentLines -= 1;
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
        file.seek(file.size() - 1);
        QByteArray endChar = file.read(1);
        if(endChar == "\n") {
            result.blankLines += 1;
        }
        result.fileName = fileInfo.fileName();
        result.path = fileInfo.absoluteFilePath();
        file.close();

        result.totalLines = result.blankLines + result.codeLines + result.commentLines;
        fileResultList.append(result);
        if(isDebugfileName) {
            qDebug() << result.fileName << "?????????" << result.totalLines;
            qDebug() << result.fileName << "???????????????" << result.codeLines;
            qDebug() << result.fileName << "???????????????" << result.commentLines;
            qDebug() << result.fileName << "???????????????" << result.blankLines;
        }
        pthread_mutex_lock(&mutex);
        allFileResult.totalLines += result.totalLines;
        allFileResult.codeLines += result.codeLines;
        allFileResult.commentLines += result.commentLines;
        allFileResult.blankLines += result.blankLines;
        pthread_mutex_unlock(&mutex);
    }
    pthread_exit(NULL);
}
void CreateThread() {
    int nFileCount = 0;
    if(allFileList.count() > THREADCOUNT) {
        nFileCount = allFileList.count() / THREADCOUNT;
    } else {
        nFileCount = 1;
    }
    struct ThreadParams* params = new ThreadParams[THREADCOUNT];
    int count = 0;
    for(int i = 0; i < THREADCOUNT && i < allFileList.count(); i++) {
        for(int j = 0; j < nFileCount; j++) {
            params[i].list.append(allFileList.at(i * nFileCount + j));
            count += 1;
        }
    }
    if(count < allFileList.count()) {
        for(int i = count; i < allFileList.count(); i++) {
            params[THREADCOUNT - 1].list.append(allFileList.at(i));
        }
    }

    pthread_t ThredPool[THREADCOUNT] = {0};
    for(int i = 0; i < THREADCOUNT; i++) {
        int  ret = pthread_create(&ThredPool[i], NULL, ProcessData, (void*)&params[i]);
        if(ret < 0) {
            qDebug() << "Create Thread Error!" << endl;
        }
    }

    for(int index = 0; index < THREADCOUNT; index++) {
        pthread_join(ThredPool[index], NULL);
    }
    if(params) {
        delete [] params;
        params = nullptr;
    }
    qDebug() << "???????????????" << allFileList.count();
    qDebug() << "??????????????????" << allFileResult.totalLines;
    qDebug() << "??????????????????" << allFileResult.codeLines;
    qDebug() << "??????????????????" << allFileResult.commentLines;
    qDebug() << "??????????????????" << allFileResult.blankLines;
    qDebug() << "?????????????????????" << filterFileType;
    exit(0);
}
void ReadFiles(QString dirPath, QStringList filter) {
    QDir dir(dirPath);
    if(!dir.exists()) {
        qDebug() << "Dir not exists.";
        exit(-1);
    }
    QStringList strings;
    strings << "*";
    QFileInfoList list = dir.entryInfoList(strings, QDir::AllEntries | QDir::NoDotAndDotDot, QDir::DirsFirst); //list??????????????????????????????
    if(list.isEmpty()) {
        if(isDebugfileName) {
            qDebug() << "list is empty.";
        }
    }
    foreach(const QFileInfo &file, list) {
        if(file.isFile()) {
            if(!file.fileName().contains("_")) {
                bool contains = std::any_of(filter.constBegin(), filter.constEnd(), [ = ](QString str) {
                    if(file.suffix().isEmpty()) {
                        return false;
                    }
                    if(str == QString("*.*")) {
                        return true;
                    }
                    return str.contains(file.suffix(), Qt::CaseInsensitive);
                });
                if(contains) {
                    if(file.suffix() == "cs" && file.fileName().contains("AssemblyInfo")) {
                        continue;
                    }
                    allFileList.append(file);
                    if(isDebugfileName) {
                        qDebug() << file.fileName();
                    }
                }
            }
        } else if(file.isDir()) {
            if(isDebugfileName) {
                qDebug() << "Dir" << file.filePath() << "'s file list as below:";
            }
            ReadFiles(file.filePath(), filter);
        }
    }
}
int main(int argc, char* argv[]) {
    qDebug() << "welcome to " << VERSION << endl;
    QCoreApplication a(argc, argv);
    pthread_mutex_lock(&mutex);
    allFileResult.fileName = QString("");
    allFileResult.path = QString("");
    allFileResult.totalLines = 0;
    allFileResult.blankLines = 0;
    allFileResult.codeLines = 0;
    allFileResult.commentLines = 0;
    pthread_mutex_unlock(&mutex);
    QString dirPath(QCoreApplication::applicationDirPath());
    QStringList filter;

    if(argc > 1) {
        dirPath = QString(argv[1]);
    } else {
        qDebug() << "???????????????????????????????????????!";
    }
    if(argc <= 2) {
        filter << ".cpp" << ".h" << ".cs";
    }
    if(argc > 2) {
        for(int i = 2; i < argc; i++) {
            filter << QString(argv[i]);
        }
    }
    if(QString(argv[argc - 1]) == "true" || QString(argv[argc - 1]) == "false") {
        if(QString(argv[argc - 1]) == "true") {
            isDebugfileName = true;
        } else {
            isDebugfileName = false;
        }
        filter.removeLast();
    }
    for(const auto& str : filter) {
        filterFileType.append(str).append(" ");
    }
    ReadFiles(dirPath, filter);
    CreateThread();
    return a.exec();
}
