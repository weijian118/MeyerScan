#pragma once
#include<QString>
#include<QJsonObject>
#include<iostream>
#include<vector>
extern double nfaktoW;
extern double nfaktoH;
extern QString APPIconPath;
//系统语言
#define G_LANG_ENGLISH                  0    //英语
#define G_LANG_SIMPLIFIED_CHINESE       1    //简体中文
#define G_LANG_SPANISH                  2    //西班牙语
#define G_LANG_FRENCH                   3    //法语
#define G_LANG_POTUGESE                 4    //葡萄牙语
#define G_LANG_PERSIAN                  5    //波斯语.
#define G_LANG_RUSSIAN                  6    //俄语
#define G_LANG_TRADITIONAL_CHINESE      7    //繁体中文

enum status_Login
{
    LOGIN_SUCCESS = 1,//登陆成功
    NOT_INPUT_ACCOUNT = 2,//未输入账户名
    NOT_INPUT_PASSWORD = 3,//未输入账户密码
    NOT_INPUT_PHONENUMBER = 4,//未输入手机号
    NOT_INPUT_OTP = 5,//未输入验证码
    USER_CANCEL_LOGIN = 6,//用户取消登陆
    CHECK_SMS_FILED=7,//验证码校验失败
    GET_SMS_FILED = 8,//获取验证码失败
    UPDATE_ACCOUNTFLAGS_FAILED=9,//更新本地文件错误
    WRITE_ACCOUNTINFO_FILED=10,//写入本地文件错误
    GET_CORPNAMES_FILED =11,//获取诊所失败
    GET_LOGINEDACCOUNTINFO_FAILED=12,//获取登录账户信息错误
    GET_CURRENTUSERORGANDDEVICELIST_FAILED=13,//获取当前用户信息列表错误
    CANCEL_OFFLINELOGIN=14,//用户取消离线登录
    WRITECLOUDMSG_SUCCESS=15//写入本地文件成功
};

struct LoginReturnParameters {
    int currentStatus;//登录状态
    QString currentAccountName;//账户名
    QString currentAccountPassword;//账户密码
    QString currentClinicId;//所属诊所ID
    QString currentClinicName;//所属诊所名
    int currentPersonId;//昵称ID
    QString currentPersonName;//昵称
    int currentCorpid;//所属诊所ID
    QString currentToken;//token
    QString currentRefreshToken;//refreshToken
    QString currentPhoneNumber;//手机号
    int loginMode;//登录模式  0:账户密码登录-在线  1:账户密码登录-离线  2:短信验证码登录  3:自动登录-在线  4:自动登录-离线  5:默认离线登录
    QString htmlReturnCode460;//460返回值msg
};

struct UserLoginParameters
{
    double nfaktoW;//分辨率系数，默认:1.0
    double nfaktoH;//分辨率系数，默认： 1.0
    QString dataPath;//保存本地文件路径（包含文件名） 默认:"Data/LocalLoginFile_rebuild.json";
    int languageType;//语言类型，默认:中文
    QString AppPath;//meyerScan软件安装路径
    QString loginUrl;//登录url,默认:开发环境
                                                        //"https://dev-ks-api.myyun.com:553";//开发环境
                                                        //"https://qa-ks-api.myyun.com";//测试环境
                                                        //"https://api-myscan.meyerop.com";//生产环境
    QString registerUrl;//注册url
                                                        //默认:"https://dev-myscan.myyun.com/login"
    int version;//80:国外版本，隐藏注册和验证码登录方式；100： 国内版本
};

struct AccountInfo
{
    std::string password;
    int rememberCodeFlag;
    int lastTimeLogin;
};
