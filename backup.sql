-- MySQL dump 10.13  Distrib 8.0.45, for Linux (x86_64)
--
-- Host: localhost    Database: chat_system
-- ------------------------------------------------------
-- Server version	8.0.45-0ubuntu0.22.04.1

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!50503 SET NAMES utf8mb4 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `sys_friend_relation`
--

DROP TABLE IF EXISTS `sys_friend_relation`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `sys_friend_relation` (
  `relation_id` bigint NOT NULL AUTO_INCREMENT COMMENT '关系ID',
  `user_id` bigint NOT NULL COMMENT '用户ID',
  `friend_id` bigint NOT NULL COMMENT '好友ID',
  `create_time` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '建立时间',
  PRIMARY KEY (`relation_id`),
  UNIQUE KEY `uk_user_friend` (`user_id`,`friend_id`),
  KEY `idx_friend_id` (`friend_id`),
  CONSTRAINT `fk_friend_friend` FOREIGN KEY (`friend_id`) REFERENCES `sys_user` (`user_id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `fk_friend_user` FOREIGN KEY (`user_id`) REFERENCES `sys_user` (`user_id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=13 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='好友关系表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `sys_friend_relation`
--

LOCK TABLES `sys_friend_relation` WRITE;
/*!40000 ALTER TABLE `sys_friend_relation` DISABLE KEYS */;
INSERT INTO `sys_friend_relation` VALUES (1,1,2,'2026-04-18 12:12:13'),(2,2,1,'2026-04-18 12:12:13'),(5,2,3,'2026-04-18 12:12:13'),(6,3,2,'2026-04-18 12:12:13'),(7,4,1,'2026-04-19 07:13:02'),(8,1,4,'2026-04-19 07:13:02'),(9,3,4,'2026-04-19 09:07:38'),(10,4,3,'2026-04-19 09:07:38'),(11,1,3,'2026-04-19 09:36:42'),(12,3,1,'2026-04-19 09:36:42');
/*!40000 ALTER TABLE `sys_friend_relation` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `sys_friend_request`
--

DROP TABLE IF EXISTS `sys_friend_request`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `sys_friend_request` (
  `request_id` bigint NOT NULL AUTO_INCREMENT COMMENT '好友申请ID',
  `from_user_id` bigint NOT NULL COMMENT '申请人ID',
  `to_user_id` bigint NOT NULL COMMENT '被申请人ID',
  `message` varchar(255) NOT NULL DEFAULT '' COMMENT '留言',
  `status` tinyint NOT NULL DEFAULT '0' COMMENT '0待处理 1同意 2拒绝',
  `create_time` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '申请时间',
  PRIMARY KEY (`request_id`),
  KEY `idx_from_user_id` (`from_user_id`),
  KEY `idx_to_user_id` (`to_user_id`),
  KEY `idx_status` (`status`),
  CONSTRAINT `fk_friend_req_from_user` FOREIGN KEY (`from_user_id`) REFERENCES `sys_user` (`user_id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `fk_friend_req_to_user` FOREIGN KEY (`to_user_id`) REFERENCES `sys_user` (`user_id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=5 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='好友申请表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `sys_friend_request`
--

LOCK TABLES `sys_friend_request` WRITE;
/*!40000 ALTER TABLE `sys_friend_request` DISABLE KEYS */;
INSERT INTO `sys_friend_request` VALUES (1,4,1,'你好',1,'2026-04-19 06:29:45'),(2,3,4,'你好',2,'2026-04-19 09:07:27'),(3,3,4,'',1,'2026-04-19 09:07:36'),(4,1,3,'你好',1,'2026-04-19 09:36:39');
/*!40000 ALTER TABLE `sys_friend_request` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `sys_group`
--

DROP TABLE IF EXISTS `sys_group`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `sys_group` (
  `group_id` bigint NOT NULL AUTO_INCREMENT COMMENT '群ID',
  `group_name` varchar(100) NOT NULL COMMENT '群名称',
  `creator_id` bigint NOT NULL COMMENT '创建者ID',
  `create_time` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
  `notice` varchar(500) DEFAULT '' COMMENT '群公告',
  `avatar` varchar(255) NOT NULL DEFAULT '' COMMENT '群头像路径',
  PRIMARY KEY (`group_id`),
  KEY `idx_creator_id` (`creator_id`),
  CONSTRAINT `fk_group_creator` FOREIGN KEY (`creator_id`) REFERENCES `sys_user` (`user_id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=4 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='群信息表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `sys_group`
--

LOCK TABLES `sys_group` WRITE;
/*!40000 ALTER TABLE `sys_group` DISABLE KEYS */;
INSERT INTO `sys_group` VALUES (1,'C++学习群',1,'2026-04-18 12:12:13','欢迎学习C++',''),(2,'Linux交流群',2,'2026-04-18 12:12:13','一起学习Linux',''),(3,'学习群',1,'2026-04-19 07:12:41','','');
/*!40000 ALTER TABLE `sys_group` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `sys_group_invite`
--

DROP TABLE IF EXISTS `sys_group_invite`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `sys_group_invite` (
  `invite_id` bigint NOT NULL AUTO_INCREMENT COMMENT '群邀请ID',
  `group_id` bigint NOT NULL COMMENT '群ID',
  `inviter_id` bigint NOT NULL COMMENT '邀请人ID',
  `invitee_id` bigint NOT NULL COMMENT '被邀请人ID',
  `message` varchar(255) NOT NULL DEFAULT '' COMMENT '邀请留言',
  `status` tinyint NOT NULL DEFAULT '0' COMMENT '0待处理 1同意 2拒绝',
  `create_time` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '邀请时间',
  PRIMARY KEY (`invite_id`),
  KEY `idx_group_id` (`group_id`),
  KEY `idx_inviter_id` (`inviter_id`),
  KEY `idx_invitee_id` (`invitee_id`),
  KEY `idx_status` (`status`),
  CONSTRAINT `fk_group_invite_group` FOREIGN KEY (`group_id`) REFERENCES `sys_group` (`group_id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `fk_group_invite_invitee` FOREIGN KEY (`invitee_id`) REFERENCES `sys_user` (`user_id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `fk_group_invite_inviter` FOREIGN KEY (`inviter_id`) REFERENCES `sys_user` (`user_id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=6 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='群邀请表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `sys_group_invite`
--

LOCK TABLES `sys_group_invite` WRITE;
/*!40000 ALTER TABLE `sys_group_invite` DISABLE KEYS */;
INSERT INTO `sys_group_invite` VALUES (1,3,1,3,'邀请你加入群聊',2,'2026-04-19 08:51:43'),(2,3,1,3,'邀请你加入群聊',1,'2026-04-19 08:51:56'),(3,3,1,3,'邀请你加入群聊',0,'2026-04-19 08:52:35'),(4,3,1,3,'邀请你加入群聊',2,'2026-04-19 08:52:59'),(5,3,1,3,'邀请你加入群聊',1,'2026-04-19 08:54:40');
/*!40000 ALTER TABLE `sys_group_invite` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `sys_group_member`
--

DROP TABLE IF EXISTS `sys_group_member`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `sys_group_member` (
  `member_id` bigint NOT NULL AUTO_INCREMENT COMMENT '成员记录ID',
  `group_id` bigint NOT NULL COMMENT '群ID',
  `user_id` bigint NOT NULL COMMENT '用户ID',
  `role` tinyint NOT NULL DEFAULT '0' COMMENT '角色(0普通成员 1管理员 2群主)',
  `join_time` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '加入时间',
  `mute_status` tinyint NOT NULL DEFAULT '0' COMMENT '禁言状态(0正常 1禁言)',
  PRIMARY KEY (`member_id`),
  UNIQUE KEY `uk_group_user` (`group_id`,`user_id`),
  KEY `idx_group_member_user` (`user_id`),
  CONSTRAINT `fk_group_member_group` FOREIGN KEY (`group_id`) REFERENCES `sys_group` (`group_id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `fk_group_member_user` FOREIGN KEY (`user_id`) REFERENCES `sys_user` (`user_id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=10 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='群成员表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `sys_group_member`
--

LOCK TABLES `sys_group_member` WRITE;
/*!40000 ALTER TABLE `sys_group_member` DISABLE KEYS */;
INSERT INTO `sys_group_member` VALUES (1,1,1,2,'2026-04-18 12:12:13',0),(3,1,3,1,'2026-04-18 12:12:13',0),(4,2,2,2,'2026-04-18 12:12:13',0),(5,2,3,0,'2026-04-18 12:12:13',0),(6,2,4,0,'2026-04-18 12:12:13',1),(7,3,1,2,'2026-04-19 07:12:41',0),(9,3,3,1,'2026-04-19 08:54:42',0);
/*!40000 ALTER TABLE `sys_group_member` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `sys_message`
--

DROP TABLE IF EXISTS `sys_message`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `sys_message` (
  `msg_id` bigint NOT NULL AUTO_INCREMENT COMMENT '消息ID',
  `sender_id` bigint NOT NULL COMMENT '发送者ID',
  `receiver_id` bigint DEFAULT NULL COMMENT '接收者ID，私聊时使用',
  `group_id` bigint DEFAULT NULL COMMENT '群ID，群聊时使用',
  `chat_type` varchar(20) NOT NULL COMMENT '聊天类型：friend/group',
  `msg_type` tinyint NOT NULL DEFAULT '1' COMMENT '消息类型：1文本 2图片 3文件',
  `content` text NOT NULL COMMENT '消息内容，文本或base64内容',
  `filename` varchar(255) NOT NULL DEFAULT '' COMMENT '文件名或图片名',
  `send_time` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '发送时间',
  `msg_status` tinyint NOT NULL DEFAULT '0' COMMENT '状态：0未读 1已读',
  PRIMARY KEY (`msg_id`),
  KEY `idx_sender_id` (`sender_id`),
  KEY `idx_receiver_id` (`receiver_id`),
  KEY `idx_group_id` (`group_id`),
  KEY `idx_chat_type` (`chat_type`),
  KEY `idx_send_time` (`send_time`),
  CONSTRAINT `fk_sys_message_group` FOREIGN KEY (`group_id`) REFERENCES `sys_group` (`group_id`) ON DELETE SET NULL ON UPDATE CASCADE,
  CONSTRAINT `fk_sys_message_receiver` FOREIGN KEY (`receiver_id`) REFERENCES `sys_user` (`user_id`) ON DELETE SET NULL ON UPDATE CASCADE,
  CONSTRAINT `fk_sys_message_sender` FOREIGN KEY (`sender_id`) REFERENCES `sys_user` (`user_id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=37 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='聊天消息表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `sys_message`
--

LOCK TABLES `sys_message` WRITE;
/*!40000 ALTER TABLE `sys_message` DISABLE KEYS */;
INSERT INTO `sys_message` VALUES (1,1,2,NULL,'friend',1,'你好李四','','2026-04-18 12:12:13',1),(2,2,1,NULL,'friend',1,'你好张三','','2026-04-18 12:12:13',1),(3,1,3,NULL,'friend',1,'王五在吗？','','2026-04-18 12:12:13',0),(4,1,NULL,1,'group',1,'大家好，这是C++群','','2026-04-18 12:12:13',1),(5,2,NULL,1,'group',1,'收到！','','2026-04-18 12:12:13',1),(6,3,NULL,1,'group',1,'我来了','','2026-04-18 12:12:13',1),(7,2,NULL,2,'group',1,'Linux很强','','2026-04-18 12:12:13',1),(8,1,2,NULL,'friend',1,'你好','','2026-04-18 12:19:43',0),(9,1,3,NULL,'friend',1,'你好','','2026-04-18 12:19:50',0),(10,2,1,NULL,'friend',1,'你好','','2026-04-18 12:20:16',0),(11,2,NULL,1,'group',1,'测试消息','','2026-04-18 12:20:33',0),(12,1,NULL,1,'group',1,'测试','','2026-04-18 12:20:37',0),(13,3,NULL,1,'group',1,'你好','','2026-04-18 12:26:35',0),(14,3,1,NULL,'friend',1,'我在','','2026-04-18 12:28:29',0),(15,1,3,NULL,'friend',1,'你好','','2026-04-18 12:38:47',0),(16,3,1,NULL,'friend',1,'我是','','2026-04-18 12:39:07',0),(17,1,NULL,1,'group',1,'我是','','2026-04-18 13:05:03',0),(18,3,1,NULL,'friend',1,'你没事把','','2026-04-18 13:05:45',0),(19,1,3,NULL,'friend',1,'我没事','','2026-04-18 13:06:07',0),(20,3,1,NULL,'friend',1,'你好','','2026-04-18 14:18:36',0),(21,1,3,NULL,'friend',1,'你好','','2026-04-18 14:18:42',0),(22,3,1,NULL,'friend',1,'你也好','','2026-04-18 14:25:59',0),(23,3,1,NULL,'friend',1,'你好','','2026-04-18 14:27:16',0),(24,3,1,NULL,'friend',1,'我是王五','','2026-04-18 14:27:22',0),(25,1,3,NULL,'friend',1,'你好','','2026-04-19 06:27:33',0),(26,1,2,NULL,'friend',1,'1','','2026-04-19 06:28:58',0),(27,2,NULL,1,'group',1,'你们好，我是李四','','2026-04-19 08:29:32',0),(28,2,NULL,1,'group',1,'怎么回事','','2026-04-19 08:30:10',0),(29,3,NULL,3,'group',1,'你好','','2026-04-19 08:52:15',0),(30,1,NULL,3,'group',1,'你好','','2026-04-19 08:52:20',0),(31,3,1,NULL,'friend',1,'1','','2026-04-19 08:53:18',0),(32,3,1,NULL,'friend',1,'file:///D:/code/ChatRoom/resources/BytesizeMessage.png','','2026-04-19 08:53:55',0),(33,4,1,NULL,'friend',1,'你好新朋友','','2026-04-19 09:07:49',0),(34,3,4,NULL,'friend',1,'你好','','2026-04-19 09:08:16',0),(35,1,3,NULL,'friend',1,'你好','','2026-04-19 09:37:05',0),(36,3,1,NULL,'friend',1,'你好','','2026-04-19 09:37:19',0);
/*!40000 ALTER TABLE `sys_message` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `sys_offline_message`
--

DROP TABLE IF EXISTS `sys_offline_message`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `sys_offline_message` (
  `offline_id` bigint NOT NULL AUTO_INCREMENT COMMENT '离线消息ID',
  `sender_id` bigint NOT NULL COMMENT '发送者ID',
  `receiver_id` bigint NOT NULL COMMENT '接收者ID',
  `content` text NOT NULL COMMENT '消息内容',
  `send_time` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '发送时间',
  `read_flag` tinyint NOT NULL DEFAULT '0' COMMENT '是否已读(0未读 1已读)',
  `msg_type` int NOT NULL DEFAULT '1' COMMENT '消息类型 1文字 2图片 3文件',
  `filename` varchar(255) NOT NULL DEFAULT '' COMMENT '文件名',
  PRIMARY KEY (`offline_id`),
  KEY `idx_offline_sender_id` (`sender_id`),
  KEY `idx_offline_receiver_id` (`receiver_id`),
  CONSTRAINT `fk_offline_receiver` FOREIGN KEY (`receiver_id`) REFERENCES `sys_user` (`user_id`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `fk_offline_sender` FOREIGN KEY (`sender_id`) REFERENCES `sys_user` (`user_id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=6 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='离线消息表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `sys_offline_message`
--

LOCK TABLES `sys_offline_message` WRITE;
/*!40000 ALTER TABLE `sys_offline_message` DISABLE KEYS */;
INSERT INTO `sys_offline_message` VALUES (1,1,3,'你刚刚不在线','2026-04-18 12:12:13',1,1,''),(2,2,4,'上线记得回复','2026-04-18 12:12:13',1,1,''),(3,1,2,'你好','2026-04-18 12:19:43',1,1,''),(4,1,3,'你好','2026-04-18 12:19:50',1,1,''),(5,4,1,'你好新朋友','2026-04-19 09:07:49',1,1,'');
/*!40000 ALTER TABLE `sys_offline_message` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `sys_user`
--

DROP TABLE IF EXISTS `sys_user`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `sys_user` (
  `user_id` bigint NOT NULL AUTO_INCREMENT COMMENT '用户ID',
  `account` varchar(13) NOT NULL COMMENT '账号',
  `password_hash` varchar(255) NOT NULL COMMENT '密码摘要/当前代码中实际直接存密码字符串',
  `nickname` varchar(100) NOT NULL COMMENT '昵称',
  `avatar` varchar(255) DEFAULT '',
  `register_time` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '注册时间',
  `status` tinyint NOT NULL DEFAULT '0' COMMENT '在线状态(0离线 1在线)',
  PRIMARY KEY (`user_id`),
  UNIQUE KEY `uk_account` (`account`)
) ENGINE=InnoDB AUTO_INCREMENT=6 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='用户表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `sys_user`
--

LOCK TABLES `sys_user` WRITE;
/*!40000 ALTER TABLE `sys_user` DISABLE KEYS */;
INSERT INTO `sys_user` VALUES (1,'user1','hash1','张三','/static/avatar/user_1.jpg','2026-04-18 12:12:13',0),(2,'user2','hash2','李四','/static/avatar/user_2.jpg','2026-04-18 12:12:13',0),(3,'user3','hash3','王五','/static/avatar/user_3.jpg','2026-04-18 12:12:13',0),(4,'user4','hash4','赵六','/static/avatar/user_4.jpg','2026-04-18 12:12:13',0),(5,'10000005','123456','杨国智','','2026-04-19 10:25:43',0);
/*!40000 ALTER TABLE `sys_user` ENABLE KEYS */;
UNLOCK TABLES;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2026-04-19 10:48:07
