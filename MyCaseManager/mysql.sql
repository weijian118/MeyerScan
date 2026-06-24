-- --------------------------------------------------------
-- 主机:                           127.0.0.1
-- 服务器版本:                        5.6.29 - MySQL Community Server (GPL)
-- 服务器操作系统:                      Win64
-- HeidiSQL 版本:                  9.4.0.5174
-- --------------------------------------------------------

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET NAMES utf8 */;
/*!50503 SET NAMES utf8mb4 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;


-- 导出 mscan 的数据库结构
CREATE DATABASE IF NOT EXISTS `mscan` /*!40100 DEFAULT CHARACTER SET utf8 */;
USE `mscan`;

-- 导出  表 mscan.abnormal_tbl2 结构
CREATE TABLE IF NOT EXISTS `abnormal_tbl2` (
  `ORDER_ID` varchar(255) NOT NULL,
  `PATIENT_ID` varchar(255) NOT NULL,
  `EXITFLAG` varchar(10) DEFAULT '0',
  `MAX` varchar(10) DEFAULT '0',
  `MAND` varchar(10) DEFAULT '0',
  `MAXBAR` varchar(10) DEFAULT '0',
  `MANDBAR` varchar(10) DEFAULT '0',
  `OCC` varchar(10) DEFAULT '0',
  `MAX_PATH` varchar(255) DEFAULT NULL,
  `MAND_PATH` varchar(255) DEFAULT NULL,
  `MAXBAR_PATH` varchar(255) DEFAULT NULL,
  `MANDBAR_PATH` varchar(255) DEFAULT NULL,
  `OCC_PATH` varchar(255) DEFAULT NULL,
  `MAXCUFF` varchar(10) DEFAULT '0',
  `MAXCUFF_PATH` varchar(255) DEFAULT NULL,
  `MAXPREPARE` varchar(10) DEFAULT '0',
  `MAXPREPARE_PATH` varchar(255) DEFAULT NULL,
  `MAXBARTWO` varchar(10) DEFAULT '0',
  `MAXBARTWO_PATH` varchar(255) DEFAULT NULL,
  `MAXDIFFBARONE` varchar(10) DEFAULT '0',
  `MAXDIFFBARONE_PATH` varchar(255) DEFAULT NULL,
  `MAXDIFFBARTWO` varchar(10) DEFAULT '0',
  `MAXDIFFBARTWO_PATH` varchar(255) DEFAULT NULL,
  `MANDCUFF` varchar(10) DEFAULT '0',
  `MANDCUFF_PATH` varchar(255) DEFAULT NULL,
  `MANDPREPARE` varchar(10) DEFAULT '0',
  `MANDPREPARE_PATH` varchar(255) DEFAULT NULL,
  `MANDBARTWO` varchar(10) DEFAULT '0',
  `MANDBARTWO_PATH` varchar(255) DEFAULT NULL,
  `MANDDIFFBARONE` varchar(10) DEFAULT '0',
  `MANDDIFFBARONE_PATH` varchar(255) DEFAULT NULL,
  `MANDDIFFBARTWO` varchar(10) DEFAULT '0',
  `MANDDIFFBARTWO_PATH` varchar(255) DEFAULT NULL,
  `OCCURECORD` varchar(10) DEFAULT '0',
  `OCCURECORD_PATH` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`ORDER_ID`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- 数据导出被取消选择。
-- 导出  表 mscan.clinic_tbl 结构
CREATE TABLE IF NOT EXISTS `clinic_tbl` (
  `CLINIC_ID` varchar(20) NOT NULL COMMENT '本地id',
  `CLINIC_NAME` varchar(50) NOT NULL COMMENT '云端clinicName',
  `CLINIC_CONTACT` varchar(20) DEFAULT NULL COMMENT 'headName',
  `CLINIC_TEL` varchar(20) DEFAULT NULL COMMENT 'telephone',
  `CLINIC_OLDMOBILE` varchar(255) DEFAULT NULL COMMENT '原手机号，如果手机号不改变，则不填',
  `CLINIC_EMAIL` varchar(50) DEFAULT NULL,
  `CLINIC_PASSWORD` varchar(255) DEFAULT NULL,
  `CLINIC_INSTITUTIONTYPE` varchar(255) DEFAULT '0' COMMENT '状态 0->独立机构 1->连锁机构',
  `CLINIC_CITY` varchar(255) DEFAULT NULL COMMENT '云端：city所在城市',
  `CLINIC_LOCATION` varchar(255) DEFAULT NULL COMMENT '云端：location诊所位置',
  `CLINIC_DETAILADDRESS` varchar(255) DEFAULT NULL COMMENT '云端：detailAddress诊所详细地址',
  `CLINIC_LICENSECODE` varchar(255) DEFAULT NULL COMMENT '营业执照编码',
  `CLINIC_BUSINESSLICENSE` varchar(255) DEFAULT NULL COMMENT '营业执照URL',
  `CLINIC_LOGO` varchar(255) DEFAULT NULL COMMENT '诊所LOGO URL',
  `CLINIC_INVITECODE` varchar(255) DEFAULT NULL COMMENT '邀请码',
  `CLINIC_EQUIPLICENSE` varchar(255) DEFAULT NULL COMMENT '设备编码（多个编码使用分隔符 “ ,” ， 组合为字符串）',
  `CLINIC_REMARK` varchar(255) DEFAULT NULL,
  `CLINIC_CLOUDID` varchar(255) DEFAULT NULL COMMENT '诊所云端ID',
  `CLINIC_ROLEID` varchar(255) DEFAULT NULL COMMENT '云端：roleId角色id',
  `CLINIC_CLOUD_PHONE` varchar(255) DEFAULT NULL COMMENT '云端门诊账号登录时用户名',
  `CLINIC_CLOUD_PASSWORD` varchar(255) DEFAULT NULL COMMENT '云端门诊账号登录时密码',
  `CLINIC_ADDRESS` varchar(255) DEFAULT NULL,
  `CLINIC_DEFAULT` int(2) DEFAULT '0',
  `CLINIC_OPERATORC` varchar(255) DEFAULT NULL,
  `CLINIC_OPERATORE` varchar(255) DEFAULT NULL,
  `CLOUD_OPEN_CLOUSE` varchar(255) DEFAULT '1' COMMENT '云端功能是否开启:0关闭，1开启',
  PRIMARY KEY (`CLINIC_ID`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- 数据导出被取消选择。
-- 导出  表 mscan.cloud_account_tbl 结构
CREATE TABLE IF NOT EXISTS `cloud_account_tbl` (
  `ACCOUNT_ID` varchar(255) NOT NULL,
  `ACCOUNT_NAME` varchar(255) DEFAULT NULL,
  `ACCOUNT_PASSWORD` varchar(255) DEFAULT NULL,
  `ACCOUNT_TOKEN` varchar(512) DEFAULT NULL,
  `ACCOUNT_CONTACT` varchar(255) DEFAULT NULL,
  `PHONE` varchar(255) DEFAULT NULL,
  `IS_ADMIN` varchar(10) DEFAULT '0',
  `IS_DEGAULT` varchar(10) DEFAULT '0',
  `IS_SAVE_PASSWORD` varchar(10) DEFAULT '0',
  `BLANK_ONE` varchar(255) DEFAULT NULL,
  `BLANK_TWO` varchar(255) DEFAULT NULL,
  `BLANK_THREE` varchar(255) DEFAULT NULL,
  `REFRESH_TOKEN` varchar(512) DEFAULT NULL,
  PRIMARY KEY (`ACCOUNT_ID`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- 数据导出被取消选择。
-- 导出  表 mscan.data_type_tbl 结构
CREATE TABLE IF NOT EXISTS `data_type_tbl` (
  `DATA_ID` bigint(20) NOT NULL AUTO_INCREMENT,
  `DISPLAY_NAME` varchar(50) NOT NULL COMMENT '英文名称',
  `SELECT_NAME` varchar(50) DEFAULT NULL COMMENT '中文名称',
  `DATA_DEFAULT` int(2) DEFAULT '0' COMMENT 'CAD识别码，十六进制',
  PRIMARY KEY (`DATA_ID`,`DISPLAY_NAME`)
) ENGINE=MyISAM AUTO_INCREMENT=5 DEFAULT CHARSET=utf8 COMMENT='修复体类型';

-- 数据导出被取消选择。
-- 导出  表 mscan.dentist_education 结构
CREATE TABLE IF NOT EXISTS `dentist_education` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `Enlish` varchar(255) DEFAULT NULL,
  `Chinese` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=6 DEFAULT CHARSET=utf8;

-- 数据导出被取消选择。
-- 导出  表 mscan.dentist_expertise 结构
CREATE TABLE IF NOT EXISTS `dentist_expertise` (
  `id` int(11) DEFAULT NULL,
  `Enlish` varchar(255) DEFAULT NULL,
  `Chinese` varchar(255) DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- 数据导出被取消选择。
-- 导出  表 mscan.dentist_tbl 结构
CREATE TABLE IF NOT EXISTS `dentist_tbl` (
  `DENTIST_ID` varchar(50) NOT NULL,
  `DENTIST_NAME` varchar(50) NOT NULL,
  `DENTIST_SEX` tinyint(2) DEFAULT NULL,
  `DENTIST_TEL` varchar(20) DEFAULT NULL,
  `DENTIST_PRO` varchar(50) DEFAULT NULL,
  `DENTIST_AGE` varchar(4) DEFAULT NULL,
  `DENTIST_DEFAULT` tinyint(2) DEFAULT NULL,
  `DENTIST_OPERATORC` varchar(20) DEFAULT NULL,
  `DENTIST_OPERATORE` text,
  `DENTIST_REMARKS` text,
  `CLINIC_ID` varchar(255) DEFAULT '1',
  `gender` enum('M','F','O') DEFAULT NULL,
  `birhtday` date DEFAULT NULL,
  `delete` enum('Y','N') DEFAULT 'N' COMMENT '是否在职',
  PRIMARY KEY (`DENTIST_ID`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- 数据导出被取消选择。
-- 导出  表 mscan.dentist_title 结构
CREATE TABLE IF NOT EXISTS `dentist_title` (
  `id` int(11) DEFAULT NULL,
  `Enlish` varchar(255) DEFAULT NULL,
  `Chinese` varchar(255) DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- 数据导出被取消选择。
-- 导出  表 mscan.device_info_tbl 结构
CREATE TABLE IF NOT EXISTS `device_info_tbl` (
  `DEVICE_ID` varchar(255) NOT NULL,
  `DURATION_CODE_TEMP` varchar(255) DEFAULT NULL,
  `DURATION_CODE` varchar(255) DEFAULT NULL,
  `IS_TEMP` varchar(10) DEFAULT '0',
  `IS_BLANK_ONE` varchar(10) DEFAULT '0',
  `IS_BLANK_TWO` varchar(10) DEFAULT '0',
  `BLANK_ONE` varchar(255) DEFAULT NULL,
  `BLANK_TWO` varchar(255) DEFAULT NULL,
  `BLANK_THREE` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`DEVICE_ID`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- 数据导出被取消选择。
-- 导出  表 mscan.lab_tbl 结构
CREATE TABLE IF NOT EXISTS `lab_tbl` (
  `LAB_ID` varchar(20) NOT NULL,
  `LAB_NAME` varchar(50) NOT NULL,
  `LAB_ADDRESS` varchar(100) DEFAULT NULL,
  `LAB_TEL` varchar(50) DEFAULT NULL COMMENT 'TELEPHONE',
  `LAB_CONTACT` varchar(50) DEFAULT NULL COMMENT '联系人姓名',
  `LAB_EMAIL` varchar(100) DEFAULT NULL COMMENT '技工所电子邮箱',
  `DEFAULT_LAB` tinyint(1) NOT NULL DEFAULT '0' COMMENT '上次选择的技工所。有且只能有一项为1',
  `LAB_REMARKS` text,
  `LAB_OPERATORC` varchar(20) DEFAULT NULL,
  `LAB_OPERATORE` text,
  PRIMARY KEY (`LAB_ID`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT='技工所';

-- 数据导出被取消选择。
-- 导出  表 mscan.lab_tbl2 结构
CREATE TABLE IF NOT EXISTS `lab_tbl2` (
  `LAB_ID` varchar(20) NOT NULL,
  `LAB_NAME` varchar(50) NOT NULL,
  `LAB_ADDRESS` varchar(100) DEFAULT NULL,
  `LAB_TEL` varchar(50) DEFAULT NULL COMMENT 'TELEPHONE',
  `LAB_CONTACT` varchar(50) DEFAULT NULL COMMENT '联系人姓名',
  `LAB_EMAIL` varchar(100) DEFAULT NULL COMMENT '技工所电子邮箱',
  `DEFAULT_LAB` tinyint(1) DEFAULT '0' COMMENT '上次选择的技工所。有且只能有一项为1',
  `LAB_REMARKS` text,
  `LAB_OPERATORC` varchar(20) DEFAULT NULL,
  `LAB_OPERATORE` text,
  `LAB_CLOUDID` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`LAB_ID`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT='技工所';

-- 数据导出被取消选择。
-- 导出  表 mscan.mask_color_tbl 结构
CREATE TABLE IF NOT EXISTS `mask_color_tbl` (
  `maskID` varchar(20) NOT NULL,
  `isAdd` varchar(5) DEFAULT '0',
  `isChecked` varchar(5) DEFAULT '0',
  `colorH` varchar(10) DEFAULT '0',
  `colorS` varchar(10) DEFAULT '0',
  `colorV` varchar(10) DEFAULT '0',
  `colorSddH` varchar(10) DEFAULT '0',
  `colorSddS` varchar(10) DEFAULT '0',
  `colorSddV` varchar(10) DEFAULT '0',
  `colorR` varchar(10) DEFAULT '0',
  `colorG` varchar(10) DEFAULT '0',
  `colorB` varchar(10) DEFAULT '0',
  PRIMARY KEY (`maskID`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- 数据导出被取消选择。
-- 导出  表 mscan.meyer_scan 结构
CREATE TABLE IF NOT EXISTS `meyer_scan` (
  `NAME` varchar(255) DEFAULT NULL,
  `VERSION` varchar(255) DEFAULT NULL,
  `COMPANY` varchar(255) DEFAULT NULL,
  `AUTH_NUMBER` varchar(255) DEFAULT NULL,
  `VAILD_PERIOD` varchar(255) DEFAULT NULL,
  `COPYRIGHT` varchar(255) DEFAULT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- 数据导出被取消选择。
-- 导出  表 mscan.order_continue_scan_tbl 结构
CREATE TABLE IF NOT EXISTS `order_continue_scan_tbl` (
  `ORDER_ID` varchar(255) NOT NULL,
  `PATIENT_ID` varchar(255) NOT NULL,
  `ORDER_SAVE_PATH` varchar(255) NOT NULL,
  `ORDER_DATE` varchar(255) NOT NULL,
  `MAX` varchar(10) DEFAULT '0',
  `MAND` varchar(10) DEFAULT '0',
  `MAXBAR` varchar(10) DEFAULT '0',
  `MANDBAR` varchar(10) DEFAULT '0',
  `OCC` varchar(10) DEFAULT '0',
  `MAX_PATH` varchar(255) DEFAULT NULL,
  `MAND_PATH` varchar(255) DEFAULT NULL,
  `MAXBAR_PATH` varchar(255) DEFAULT NULL,
  `MANDBAR_PATH` varchar(255) DEFAULT NULL,
  `OCC_PATH` varchar(255) DEFAULT NULL,
  `MAXCUFF` varchar(10) DEFAULT '0',
  `MAXCUFF_PATH` varchar(255) DEFAULT NULL,
  `MAXPREPARE` varchar(10) DEFAULT '0',
  `MAXPREPARE_PATH` varchar(255) DEFAULT NULL,
  `MAXBARTWO` varchar(10) DEFAULT '0',
  `MAXBARTWO_PATH` varchar(255) DEFAULT NULL,
  `MAXDIFFBARONE` varchar(10) DEFAULT '0',
  `MAXDIFFBARONE_PATH` varchar(255) DEFAULT NULL,
  `MAXDIFFBARTWO` varchar(10) DEFAULT '0',
  `MAXDIFFBARTWO_PATH` varchar(255) DEFAULT NULL,
  `MANDCUFF` varchar(10) DEFAULT '0',
  `MANDCUFF_PATH` varchar(255) DEFAULT NULL,
  `MANDPREPARE` varchar(10) DEFAULT '0',
  `MANDPREPARE_PATH` varchar(255) DEFAULT NULL,
  `MANDBARTWO` varchar(10) DEFAULT '0',
  `MANDBARTWO_PATH` varchar(255) DEFAULT NULL,
  `MANDDIFFBARONE` varchar(10) DEFAULT '0',
  `MANDDIFFBARONE_PATH` varchar(255) DEFAULT NULL,
  `MANDDIFFBARTWO` varchar(10) DEFAULT '0',
  `MANDDIFFBARTWO_PATH` varchar(255) DEFAULT NULL,
  `OCCURECORD` varchar(10) DEFAULT '0',
  `OCCURECORD_PATH` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`ORDER_ID`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- 数据导出被取消选择。
-- 导出  表 mscan.order_prosthesis_tbl 结构
CREATE TABLE IF NOT EXISTS `order_prosthesis_tbl` (
  `ID` bigint(20) NOT NULL AUTO_INCREMENT,
  `ORDER_ID` varchar(50) NOT NULL COMMENT '所属订单号',
  `PROSTHESIS_CODE` varchar(100) NOT NULL COMMENT '修复体编号，默认编号系统为UNN',
  `TYPE_ID` bigint(20) NOT NULL COMMENT '修复体类型ID',
  `MATERIAL_ID` bigint(20) NOT NULL COMMENT '修复体材料ID',
  `COLOR_ID` bigint(20) NOT NULL COMMENT '修复体颜色ID',
  `PONTIC_TYPE_ID` bigint(20) NOT NULL DEFAULT '0' COMMENT '修复体桥体类型，若为0则非桥体',
  `CODE_SYSTEM` tinyint(1) NOT NULL COMMENT '牙齿编号系统',
  PRIMARY KEY (`ID`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT='订单所包含修复体表格';

-- 数据导出被取消选择。
-- 导出  表 mscan.order_prosthesis_tbl2 结构
CREATE TABLE IF NOT EXISTS `order_prosthesis_tbl2` (
  `ID` bigint(20) NOT NULL AUTO_INCREMENT,
  `ORDER_ID` varchar(50) NOT NULL COMMENT '所属订单号',
  `PROSTHESIS_CODE` varchar(100) DEFAULT NULL COMMENT '修复体编号，默认编号系统为UNN',
  `TYPE_ID` bigint(20) DEFAULT NULL COMMENT '修复体类型ID',
  `MATERIAL_ID` bigint(20) DEFAULT NULL COMMENT '修复体材料ID',
  `COLOR_ID` bigint(20) DEFAULT NULL COMMENT '修复体颜色ID',
  `PONTIC_TYPE_ID` bigint(20) DEFAULT '0' COMMENT '修复体桥体类型，若为0则非桥体',
  `CODE_SYSTEM` tinyint(1) DEFAULT NULL COMMENT '牙齿编号系统',
  PRIMARY KEY (`ID`,`ORDER_ID`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT='订单所包含修复体表格';

-- 数据导出被取消选择。
-- 导出  表 mscan.order_restoration_tbl2 结构
CREATE TABLE IF NOT EXISTS `order_restoration_tbl2` (
  `ORDER_ID` varchar(50) NOT NULL,
  `RESTORATION_ID` varchar(50) NOT NULL,
  `TYPE` varchar(255) NOT NULL,
  `MATERIAL` varchar(255) DEFAULT NULL,
  `COLOR` varchar(255) DEFAULT NULL,
  `CLOUDID` varchar(255) DEFAULT NULL COMMENT '云端ID',
  PRIMARY KEY (`ORDER_ID`,`RESTORATION_ID`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- 数据导出被取消选择。
-- 导出  表 mscan.order_tbl 结构
CREATE TABLE IF NOT EXISTS `order_tbl` (
  `ORDER_ID` varchar(50) NOT NULL COMMENT '订单ID',
  `APPOINT_DATE` date NOT NULL COMMENT '预约日期',
  `APPOINT_TIME` time NOT NULL COMMENT '预约时间',
  `PATIENT_ID` varchar(50) NOT NULL,
  `PATIENT_NAME` varchar(50) NOT NULL,
  `LAB_ID` bigint(20) NOT NULL COMMENT '技工所ID',
  `DELIVERY_DATE` date NOT NULL COMMENT '交付日期',
  `DENTIST_ID` bigint(20) NOT NULL COMMENT '牙医ID，即user_tbl',
  `SAVE_PATH` varchar(255) NOT NULL DEFAULT '' COMMENT '重建结果保存路径',
  `ORDER_STATE` int(1) NOT NULL DEFAULT '0' COMMENT '订单状态',
  `REMARK` varchar(500) NOT NULL DEFAULT '' COMMENT '备注',
  PRIMARY KEY (`ORDER_ID`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT='订单';

-- 数据导出被取消选择。
-- 导出  表 mscan.order_tbl2 结构
CREATE TABLE IF NOT EXISTS `order_tbl2` (
  `ORDER_ID` varchar(50) NOT NULL,
  `APPOINT_DATE` date DEFAULT NULL,
  `APPOINT_TIEM` time DEFAULT NULL,
  `PATIENT_ID` varchar(50) NOT NULL,
  `PATIENT_NAME` varchar(255) DEFAULT NULL,
  `LAB_ID` varchar(50) DEFAULT NULL,
  `DELIVERY_DATE` varchar(50) DEFAULT NULL,
  `DENTIST_ID` bigint(20) DEFAULT NULL,
  `SAVE_PATH` varchar(255) DEFAULT NULL,
  `ORDER_STATE` int(1) DEFAULT NULL,
  `REMARK` varchar(500) DEFAULT NULL,
  `ORDER_TYPE` varchar(8) DEFAULT NULL,
  `ORDER_PLANTING_TYPE` int(2) DEFAULT NULL,
  `ORDER_OPERATORC` varchar(255) DEFAULT NULL,
  `ORDER_OPERATORE` text,
  `ORDER_DATE` varchar(8) DEFAULT NULL,
  `ORDER_TIME` varchar(8) DEFAULT NULL,
  `SEND_DATETIME` varchar(20) DEFAULT NULL,
  `PLANTING_TYPE` int(4) DEFAULT NULL,
  `DENTIST_CLOUDID` varchar(255) DEFAULT NULL COMMENT '医生云端ID',
  `PATIENT_CLOUDID` varchar(255) DEFAULT NULL COMMENT '患者云端ID',
  `CLOUDORDERID` int(11) DEFAULT NULL COMMENT '云端订单ID',
  `ORDER_ISCOMPETE` int(11) DEFAULT '0' COMMENT '记录扫描数据是否为完成颌',
  `TOOTH_READY` varchar(4) DEFAULT '0',
  `MAX_PONTIC` varchar(1024) DEFAULT NULL,
  `MAND_PONTIC` varchar(1024) DEFAULT NULL,
  `MYCLOUD_PATIENT_ID` varchar(512) DEFAULT NULL,
  `MYCLOUD_ORDER_ID` varchar(512) DEFAULT NULL,
  `DEVICE_ID` varchar(512) DEFAULT NULL,
  `DEVICE_SECRETID` varchar(512) DEFAULT NULL,
  `MYCLOUD_CLINIC_ID` varchar(512) DEFAULT NULL,
  `MAX_TRAN` mediumtext,
  `MAND_TRAN` mediumtext,
  `MAXBAR_TRAN` mediumtext,
  `MANDBAR_TRAN` mediumtext,
  `MYCLOUD_SEND_LAB_ID` varchar(512) DEFAULT NULL,
  `ORDER_SEND_LAB_NAME` varchar(512) DEFAULT NULL,
  `MANDPREPARE_TRAN` mediumtext,
  `MAXPREPARE_TRAN` mediumtext,
  `MANDCUFF_TRAN` mediumtext,
  `MAXCUFF_TRAN` mediumtext,
  `MANDBARTWO_TRAN` mediumtext,
  `MAXBARTWO_TRAN` mediumtext,
  `MANDDIFFBARONE_TRAN` mediumtext,
  `MAXDIFFBARONE_TRAN` mediumtext,
  `MANDDIFFBARTWO_TRAN` mediumtext,
  `MAXDIFFBARTWO_TRAN` mediumtext,
  `OCCLUSIONRECORD_TRAN` mediumtext,
  `ACCESSION_NUMBER` varchar(512) DEFAULT NULL,
  `PHYSICIAN_NAME` varchar(512) DEFAULT NULL,
  `STUDY_DATE` varchar(512) DEFAULT NULL,
  `STUDY_TIME` varchar(512) DEFAULT NULL,
  PRIMARY KEY (`ORDER_ID`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- 数据导出被取消选择。
-- 导出  表 mscan.patient_tbl 结构
CREATE TABLE IF NOT EXISTS `patient_tbl` (
  `PATIENT_ID` varchar(50) NOT NULL,
  `PATIENT_NAME` varchar(50) NOT NULL,
  `PATIENT_SEX` tinyint(1) NOT NULL COMMENT '1:MALE; 0:FEMALE',
  `PATIENT_DOB` date NOT NULL COMMENT 'DATE OF BIRTH',
  `PATIENT_PHONE` varchar(50) NOT NULL,
  `PATIENT_EMAIL` varchar(100) DEFAULT NULL,
  `PATIENT_ADDRESS` varchar(100) DEFAULT NULL,
  `PATIENT_ORDERCOUNTS` int(11) DEFAULT '0',
  PRIMARY KEY (`PATIENT_ID`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- 数据导出被取消选择。
-- 导出  表 mscan.patient_tbl2 结构
CREATE TABLE IF NOT EXISTS `patient_tbl2` (
  `PATIENT_ID` varchar(64) NOT NULL,
  `PATIENT_NAME` varchar(50) NOT NULL,
  `PATIENT_GENDER` enum('M','F','O') NOT NULL DEFAULT 'O',
  `PATIENT_SEX` tinyint(1) NOT NULL COMMENT '1:MALE; 0:FEMALE; 2 unknow 性别 0->女 1->男',
  `PATIENT_DOB` date DEFAULT NULL COMMENT 'DATE OF BIRTH',
  `PATIENT_PHONE` varchar(50) NOT NULL,
  `PATIENT_EMAIL` varchar(100) DEFAULT NULL,
  `PATIENT_WEIGHT` float(12,0) DEFAULT NULL COMMENT '体重，单位kg',
  `PATIENT_ADDRESS` varchar(100) DEFAULT NULL COMMENT '账户ID',
  `PATIENT_DENTIST` varchar(20) DEFAULT NULL,
  `PATIENT_AGE` varchar(4) DEFAULT NULL,
  `PATIENT_DATE` varchar(8) DEFAULT NULL,
  `PATIENT_TIME` varchar(8) DEFAULT NULL,
  `PATIENT_OPREATORC` varchar(20) DEFAULT NULL,
  `PATIENT_OPREATORE` text,
  `PATIENT_REMARKS` text,
  `PATIENT_ORDERCOUNTS` int(11) DEFAULT NULL,
  `PATIENT_SAVE_PATH` varchar(255) DEFAULT NULL,
  `PATIENT_BIRHTDATE` varchar(255) DEFAULT NULL COMMENT '出生日期：2020-07-29',
  `PATIENT_CREATETIME` varchar(255) DEFAULT NULL COMMENT '创建时间：2020-10-12 15:12:00',
  `PATIENT_UPDATETIME` varchar(255) DEFAULT NULL COMMENT '更新时间：2020-10-12 15:12:00',
  `register_date` date DEFAULT NULL,
  `delete` enum('Y','N') CHARACTER SET utf8mb4 DEFAULT 'N' COMMENT '是否已被删除',
  `birthday` date DEFAULT NULL,
  `subordinate` varchar(64) DEFAULT NULL COMMENT '所属账户',
  `DENTIST_CLOUDID` varchar(255) DEFAULT NULL COMMENT '患者对应的医生云端ID',
  `PATIENT_CLOUDID` varchar(255) DEFAULT NULL COMMENT '患者云端ID',
  `MYCLOUD_PATIENT_ID` varchar(512) DEFAULT NULL,
  `MYCLOUD_CLINIC_ID` varchar(512) DEFAULT NULL,
  `CREATE_TYPE` varchar(4) DEFAULT '0',
  PRIMARY KEY (`PATIENT_ID`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- 数据导出被取消选择。
-- 导出  表 mscan.pontic_type_tbl 结构
CREATE TABLE IF NOT EXISTS `pontic_type_tbl` (
  `ID` bigint(20) NOT NULL AUTO_INCREMENT,
  `DISPLAY_NAME` varchar(50) NOT NULL,
  `SELECT_NAME` varchar(50) NOT NULL,
  `CAD_ID` varchar(50) NOT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=MyISAM AUTO_INCREMENT=3 DEFAULT CHARSET=utf8 COMMENT='牙桥类型';

-- 数据导出被取消选择。
-- 导出  表 mscan.prosthesis_material_tbl 结构
CREATE TABLE IF NOT EXISTS `prosthesis_material_tbl` (
  `ID` bigint(20) NOT NULL AUTO_INCREMENT,
  `DISPLAY_NAME` varchar(50) NOT NULL COMMENT '预览显示名称',
  `SELECT_NAME` varchar(50) NOT NULL COMMENT '中文名称',
  PRIMARY KEY (`ID`)
) ENGINE=MyISAM AUTO_INCREMENT=12 DEFAULT CHARSET=utf8 COMMENT='修复体材料';

-- 数据导出被取消选择。
-- 导出  表 mscan.prosthesis_material_tbl2 结构
CREATE TABLE IF NOT EXISTS `prosthesis_material_tbl2` (
  `ID` bigint(20) NOT NULL AUTO_INCREMENT,
  `DISPLAY_NAME` varchar(50) DEFAULT NULL COMMENT '预览显示名称',
  `SELECT_NAME` varchar(50) DEFAULT NULL COMMENT '中文名称',
  PRIMARY KEY (`ID`)
) ENGINE=MyISAM AUTO_INCREMENT=12 DEFAULT CHARSET=utf8 COMMENT='修复体材料';

-- 数据导出被取消选择。
-- 导出  表 mscan.prosthesis_material_type_related_tbl 结构
CREATE TABLE IF NOT EXISTS `prosthesis_material_type_related_tbl` (
  `ID` bigint(20) NOT NULL AUTO_INCREMENT,
  `MATERIAL_ID` bigint(20) NOT NULL,
  `TYPE_ID` bigint(20) NOT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=MyISAM AUTO_INCREMENT=26 DEFAULT CHARSET=utf8 COMMENT='修复体类型和材料关联表';

-- 数据导出被取消选择。
-- 导出  表 mscan.prosthesis_type_tbl 结构
CREATE TABLE IF NOT EXISTS `prosthesis_type_tbl` (
  `ID` bigint(20) NOT NULL AUTO_INCREMENT,
  `DISPLAY_NAME` varchar(50) NOT NULL COMMENT '英文名称',
  `SELECT_NAME` varchar(50) NOT NULL COMMENT '中文名称',
  `CAD_ID` varchar(50) NOT NULL COMMENT 'CAD识别码，十六进制',
  PRIMARY KEY (`ID`)
) ENGINE=MyISAM AUTO_INCREMENT=10 DEFAULT CHARSET=utf8 COMMENT='修复体类型';

-- 数据导出被取消选择。
-- 导出  表 mscan.restoration_type_tbl2 结构
CREATE TABLE IF NOT EXISTS `restoration_type_tbl2` (
  `ID` int(5) NOT NULL,
  `TYPE_E` varchar(50) NOT NULL,
  `TYPE_C` varchar(50) NOT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- 数据导出被取消选择。
-- 导出  表 mscan.soft_init 结构
CREATE TABLE IF NOT EXISTS `soft_init` (
  `ID` int(4) NOT NULL AUTO_INCREMENT,
  `SWITCH` int(4) DEFAULT '0' COMMENT '0表示正常登录，不跳过默认登录。1表示切换登录，跳过默认登录。',
  `DEFAULT_ORDER_TYPE` varchar(4) DEFAULT '0',
  `MUSIC_PATH` varchar(255) DEFAULT NULL,
  `MAXMANDAUTO_HOLEFILLING` varchar(5) DEFAULT '100',
  `BARAUTO_HOLEFILLING` varchar(5) DEFAULT '1',
  `BARSCAN_TYPE` varchar(5) DEFAULT '2',
  `SCAN_AUTO_NEXT` varchar(4) DEFAULT '0',
  `TEXTURE_CHOOSE` varchar(4) DEFAULT '0',
  `MYSCAN_AUTO_UPLOAD` varchar(4) DEFAULT '0',
  `GYROSCOPE_SWITCH` varchar(4) DEFAULT '0',
  `SCAN_GIF` varchar(4) DEFAULT '1',
  PRIMARY KEY (`ID`)
) ENGINE=MyISAM AUTO_INCREMENT=2 DEFAULT CHARSET=utf8;

-- 数据导出被取消选择。
-- 导出  表 mscan.tips_mapping_tbl 结构
CREATE TABLE IF NOT EXISTS `tips_mapping_tbl` (
  `TIPS_INDEX` varchar(100) NOT NULL,
  `SHOW_FLAG` varchar(10) DEFAULT '1',
  `TEMP_SHOW_FLAG` varchar(10) DEFAULT '1',
  PRIMARY KEY (`TIPS_INDEX`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- 数据导出被取消选择。
-- 导出  表 mscan.tooth_color_tbl 结构
CREATE TABLE IF NOT EXISTS `tooth_color_tbl` (
  `ID` bigint(20) NOT NULL AUTO_INCREMENT,
  `NAME` varchar(50) NOT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=MyISAM AUTO_INCREMENT=27 DEFAULT CHARSET=utf8 COMMENT='齿色';

-- 数据导出被取消选择。
-- 导出  表 mscan.tooth_color_tbl2 结构
CREATE TABLE IF NOT EXISTS `tooth_color_tbl2` (
  `ID` bigint(20) NOT NULL AUTO_INCREMENT,
  `NAME` varchar(50) NOT NULL,
  `SELECT_NAME` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=MyISAM AUTO_INCREMENT=10 DEFAULT CHARSET=utf8 COMMENT='齿色';

-- 数据导出被取消选择。
-- 导出  表 mscan.user_tbl 结构
CREATE TABLE IF NOT EXISTS `user_tbl` (
  `ID` bigint(20) NOT NULL AUTO_INCREMENT,
  `USER_NAME` varchar(50) NOT NULL,
  `USER_PASS` varchar(50) NOT NULL DEFAULT '',
  `USER_SEX` tinyint(1) NOT NULL DEFAULT '1' COMMENT '1:MALE; 0:FEMALE',
  `USER_CELLPHONE` varchar(50) NOT NULL DEFAULT '' COMMENT '�ֻ�����',
  `DENTIST_ADDRESS` varchar(100) NOT NULL DEFAULT '' COMMENT '��ҽ��ַ',
  `DENTIST_EMAIL` varchar(100) NOT NULL DEFAULT '' COMMENT '��������',
  `AUTOLOGIN` tinyint(1) NOT NULL DEFAULT '0',
  `ISADMIN` tinyint(1) NOT NULL DEFAULT '1',
  PRIMARY KEY (`ID`)
) ENGINE=MyISAM AUTO_INCREMENT=2 DEFAULT CHARSET=utf8 COMMENT='登陆用户表';

-- 数据导出被取消选择。
-- 导出  表 mscan.user_tbl2 结构
CREATE TABLE IF NOT EXISTS `user_tbl2` (
  `ID` bigint(20) NOT NULL AUTO_INCREMENT,
  `USER_CELLPHONE` varchar(50) DEFAULT '' COMMENT '�ֻ�����',
  `USER_PASS` varchar(50) DEFAULT '',
  `USER_NAME` varchar(50) NOT NULL,
  `USER_SEX` tinyint(1) DEFAULT '1' COMMENT '1:MALE; 0:FEMALE',
  `AUTOLOGIN` tinyint(1) DEFAULT '0',
  `DENTIST_ADDRESS` varchar(100) DEFAULT '' COMMENT '��ҽ��ַ',
  `EDUCATIONID` int(11) DEFAULT '1' COMMENT '学历 1->专科 2->本科 3->硕士 4->博士 5->其他',
  `DOCTORTITLEID` int(11) DEFAULT '1' COMMENT '职称 1->执业医师 2->主治医师 3->副主任医师 4->主任医师 5->其他',
  `EXPERTISE` int(11) DEFAULT '1' COMMENT '擅长专业:',
  `DENTIST_EMAIL` varchar(100) DEFAULT '' COMMENT '��������',
  `STATUS` int(11) DEFAULT '1' COMMENT '任职状态：1->在职 0->离职',
  `BIRHTDATE` varchar(255) DEFAULT NULL COMMENT '出生日期 2020-11-15',
  `ENTRYDATE` varchar(255) DEFAULT NULL COMMENT '入职日期 2020-11-15',
  `ISADMIN` tinyint(1) DEFAULT '0',
  `AUTHORITY` int(4) DEFAULT NULL,
  `USER_OPERATORC` varchar(20) DEFAULT NULL,
  `USER_OPERATORE` text,
  `USER_REMARKS` text,
  `uid` varchar(64) DEFAULT NULL,
  `password` varchar(255) DEFAULT NULL COMMENT 'AES加密字符串',
  `CLOUDID` varchar(255) DEFAULT NULL COMMENT '云端账户id',
  `CLOUDPASSWORD` varchar(255) DEFAULT NULL COMMENT '保存不加密密码',
  `ROLEID` varchar(255) DEFAULT NULL COMMENT '云端账户角色ID',
  `ISCLINIC` int(11) DEFAULT '0' COMMENT '是否为诊所账户:0否；1是。',
  `AVATAR` varchar(255) DEFAULT NULL COMMENT '头像',
  PRIMARY KEY (`ID`,`USER_NAME`)
) ENGINE=MyISAM AUTO_INCREMENT=2691 DEFAULT CHARSET=utf8 COMMENT='登陆用户表';

-- 数据导出被取消选择。
/*!40101 SET SQL_MODE=IFNULL(@OLD_SQL_MODE, '') */;
/*!40014 SET FOREIGN_KEY_CHECKS=IF(@OLD_FOREIGN_KEY_CHECKS IS NULL, 1, @OLD_FOREIGN_KEY_CHECKS) */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
