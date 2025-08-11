#include <QCoreApplication>
#include <QDebug>
#include <QString>
#include <QStringList>
#include <QFileInfoList>
#include <QFileInfo>
#include <QDir>
#include <QTimer>
#include <chrono>

#if defined(_WIN32) || defined(_WIN64)
#include <thread>
#include <mutex>
#else
#include "pthread.h"
#endif

#define VERSION "codeline V1.1"
#define THREADCOUNT 10
struct FileCountResult {
	QString     fileName;
	QString     path;
	int totalLines = 0;
	int codeLines = 0;
	int commentLines = 0;
	int blankLines = 0;
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
#if defined(_WIN32) || defined(_WIN64)
std::mutex mtx;
#else
pthread_mutex_t mtx;
#endif

void* ProcessData(void* para) {
	ThreadParams* param = (ThreadParams*)para;
	QList<FileCountResult> fileResultList;
	foreach(QFileInfo fileInfo, param->list) {
		FileCountResult result;
		QFile file(fileInfo.absoluteFilePath());
		if (!file.open(QIODevice::Text | QIODevice::ReadOnly)) {
			qDebug() << "open file failed" << fileInfo.absoluteFilePath();
			break;
		}
		while (!file.atEnd()) {
			QString line = file.readLine().trimmed();
			if (line.isEmpty()) {
				result.blankLines += 1;
			} else if (line.startsWith(singleLineComment)) {
				result.commentLines += 1;
			} else {
				int posmultiLineCommentsBegin = line.indexOf(multiLineCommentsBegin);
				if (posmultiLineCommentsBegin != 0) {
					result.codeLines += 1;
				} else if (posmultiLineCommentsBegin == 0) {
					result.commentLines += 1;
				}

				if (posmultiLineCommentsBegin != -1) {
					if (!line.contains(multiLineCommentsEnd)) {
						while (!file.atEnd()) {
							QString nextLine = file.readLine().trimmed();
							result.commentLines += 1;
							if (nextLine.contains(multiLineCommentsEnd)) {
								if (!nextLine.endsWith(multiLineCommentsEnd)) {
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
		if (endChar == "\n") {
			result.blankLines += 1;
		}
		result.fileName = fileInfo.fileName();
		result.path = fileInfo.absoluteFilePath();
		file.close();

		result.totalLines = result.blankLines + result.codeLines + result.commentLines;
		fileResultList.append(result);
		if (isDebugfileName) {
			qDebug().noquote() << result.fileName << QStringLiteral("总行数：") << result.totalLines;
			qDebug().noquote() << result.fileName << QStringLiteral("代码总行数：") << result.codeLines;
			qDebug().noquote() << result.fileName << QStringLiteral("注释总行数") << result.commentLines;
			qDebug().noquote() << result.fileName << QStringLiteral("空白总行数") << result.blankLines;
		}
#if defined(_WIN32) || defined(_WIN64)
		mtx.lock();
#else
		pthread_mutex_lock(&mtx);
#endif
		allFileResult.totalLines += result.totalLines;
		allFileResult.codeLines += result.codeLines;
		allFileResult.commentLines += result.commentLines;
		allFileResult.blankLines += result.blankLines;
#if defined(_WIN32) || defined(_WIN64)
		mtx.unlock();
#else
		pthread_mutex_unlock(&mtx);
#endif
	}
#if !(defined(_WIN32) || defined(_WIN64))
	pthread_exit(NULL);
#endif
	return nullptr;
}
void CreateThread() {
	int nFileCount = 0;
	if (allFileList.count() > THREADCOUNT) {
		nFileCount = allFileList.count() / THREADCOUNT;
	} else {
		nFileCount = 1;
	}
	struct ThreadParams* params = new ThreadParams[THREADCOUNT];
	int count = 0;
	for (int i = 0; i < THREADCOUNT && i < allFileList.count(); i++) {
		for (int j = 0; j < nFileCount; j++) {
			params[i].list.append(allFileList.at(i * nFileCount + j));
			count += 1;
		}
	}
	if (count < allFileList.count()) {
		for (int i = count; i < allFileList.count(); i++) {
			params[THREADCOUNT - 1].list.append(allFileList.at(i));
		}
	}
#if defined(_WIN32) || defined(_WIN64)
	std::thread ThreadPool[THREADCOUNT];
#else
	pthread_t ThreadPool[THREADCOUNT] = { 0 };
#endif
	for (int i = 0; i < THREADCOUNT; i++) {
#if defined(_WIN32) || defined(_WIN64)
		ThreadPool[i] = std::thread(ProcessData, (void*)&params[i]);
#else
		int  ret = pthread_create(&ThreadPool[i], NULL, ProcessData, (void*)&params[i]);
		if (ret < 0) {
			qDebug() << "Create Thread Error!" << endl;
		}
#endif
	}

	for (int index = 0; index < THREADCOUNT; index++) {
#if defined(_WIN32) || defined(_WIN64)
		ThreadPool[index].join();
#else
		pthread_join(ThreadPool[index], NULL);
#endif
	}
	if (params) {
		delete[] params;
		params = nullptr;
	}
	qDebug().noquote() << QStringLiteral("文件总数：") << allFileList.count();
	qDebug().noquote() << QStringLiteral("文件总行数：") << allFileResult.totalLines;
	qDebug().noquote() << QStringLiteral("代码总行数：") << allFileResult.codeLines;
	qDebug().noquote() << QStringLiteral("注释总行数：") << allFileResult.commentLines;
	qDebug().noquote() << QStringLiteral("空白总行数：") << allFileResult.blankLines;
	qDebug().noquote() << QStringLiteral("注释占比：") << QString::number((float)allFileResult.commentLines / (float)allFileResult.totalLines * 100, 'f', 2) + "%";
	qDebug().noquote() << QStringLiteral("统计文件类型：") << filterFileType;
	//	exit(0);
}
void ReadFiles(QString dirPath, QStringList filter) {
	QDir dir(dirPath);
	if (!dir.exists()) {
		qDebug() << "Dir not exists.";
		exit(-1);
	}
	QStringList strings;
	strings << "*";
	QFileInfoList list = dir.entryInfoList(strings, QDir::AllEntries | QDir::NoDotAndDotDot, QDir::DirsFirst); 
	if (list.isEmpty()) {
		if (isDebugfileName) {
			qDebug() << "list is empty.";
		}
	}
	foreach(const QFileInfo & file, list) {
		if (file.isFile()) {
			if (!file.fileName().contains("_")) {
				bool contains = std::any_of(filter.constBegin(), filter.constEnd(), [=](QString str) {
					if (file.suffix().isEmpty()) {
						return false;
					}
					if (str == QString("*.*")) {
						return true;
					}
					return str.contains(file.suffix(), Qt::CaseInsensitive);
					});
				if (contains) {
					if (file.suffix() == "cs" && file.fileName().contains("AssemblyInfo")) {
						continue;
					}
					allFileList.append(file);
					if (isDebugfileName) {
						qDebug() << file.fileName();
					}
				}
			}
		} else if (file.isDir()) {
			if (isDebugfileName) {
				qDebug() << "Dir" << file.filePath() << "'s file list as below:";
			}
			ReadFiles(file.filePath(), filter);
		} else {
			qDebug().noquote() << QStringLiteral("文件类型不支持：") << file.filePath();
		}
	}
}

int main(int argc, char* argv[]) {
	qDebug() << "welcome to " << VERSION << endl;
	QCoreApplication a(argc, argv);
#if defined(_WIN32) || defined(_WIN64)
	mtx.lock();
#else
	pthread_mutex_lock(&mtx);
#endif
	allFileResult.fileName = QString("");
	allFileResult.path = QString("");
	allFileResult.totalLines = 0;
	allFileResult.blankLines = 0;
	allFileResult.codeLines = 0;
	allFileResult.commentLines = 0;
#if defined(_WIN32) || defined(_WIN64)
	mtx.unlock();
#else
	pthread_mutex_unlock(&mtx);
#endif
	QString dirPath(QCoreApplication::applicationDirPath());
	QStringList filter;

	if (argc > 1) {
		dirPath = QString(argv[1]);
	} else {
		qDebug().noquote() <<QStringLiteral("没有指定路径，统计当前路径!");
	}
	if (argc <= 2) {
		filter << ".cpp" << ".h" << ".cs";
	}
	if (argc > 2) {
		for (int i = 2; i < argc; i++) {
			filter << QString(argv[i]);
		}
	}
	if (QString(argv[argc - 1]) == "true" || QString(argv[argc - 1]) == "false") {
		if (QString(argv[argc - 1]) == "true") {
			isDebugfileName = true;
		} else {
			isDebugfileName = false;
		}
		filter.removeLast();
	}
	for (const auto& str : filter) {
		filterFileType.append(str).append(" ");
	}
	auto t1 = std::chrono::high_resolution_clock::now();
	qDebug().noquote() << QStringLiteral("开始统计文件类型：") << filter;
	ReadFiles(dirPath, filter);
	CreateThread();
	auto t2 = std::chrono::high_resolution_clock::now();
	qDebug().noquote() << QStringLiteral("统计完成，开始输出结果...");
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
	qDebug().noquote() << QStringLiteral("统计耗时：") << duration << QStringLiteral("毫秒");
	qDebug().noquote() << QStringLiteral("统计完成，感谢使用！");
	 // 设置一个定时器，5秒后退出应用
	QTimer::singleShot(20, &a, &QCoreApplication::quit);
	return a.exec();
}
