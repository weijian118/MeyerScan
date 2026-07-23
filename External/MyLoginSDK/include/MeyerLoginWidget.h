#pragma once

#include "meyerloginwidget_global.h"
#include <QObject>
#include "globalLoginValue.h"
#include <QTranslator>

class SoftLoginD;
class LoginWidget;
class CBLMEYERLOGINWIDGET_EXPORT CBLMeyerLoginWidget :public QObject
{
	Q_OBJECT
public:
	CBLMeyerLoginWidget();

private:
	SoftLoginD*m_softlogin;
	LoginWidget*m_loginW;
	//设置登录网址
	void setLoginAndRegisterUrl(QString loginUrl, QString registerUrl);
	QTranslator *translator;
public:
	//初始化和显示登录窗口
	void initLoginWidgetAndShow(const UserLoginParameters);

	//提取信息
	//filePath:本地许可文件路径
	//accountName和cellPhone可以填充一个或者全部
	//若传corpId=-1，则返回该账户下的所有信息
	//若只传corpId，没有传deviceCode，则返回corpId下的所有信息
	//若传了corpId和deviceCode则返回该设备的所有信息
	QJsonObject findDeviceInfoFromFile(const QString& filePath, const QString& accountName, const QString& cellPhone, int corpId, const QString& deviceCode);

	//写入信息(刷新接口)
	/*token：当前账户token
	filePath:本地文件存储路径
	accountName:当前账户名
	cellPhone:当前账户手机号
	loginUrl:当前登录网址*/
	//写入成功返回WRITECLOUDMSG_SUCCESS
	status_Login writeCloudMsgtoLocal(QString token, QString filePath, QString accountName, QString cellPhone, QString loginUrl);

	//取消自动登录
	bool cancelAutoLogin(QString filePath);

	//基准时间存储和写入

	//多语言：显示之前load
	void loadTranslation(const QString &qmFilePath);
signals:
	void loginStatusReturn(const LoginReturnParameters);
public slots:
	void getLoginDialogStatus(const LoginReturnParameters);
};
