#include <iostream>
#include <conio.h>
#include "winsock.h"
#include "stdio.h"
#include "CfgFileParms.h"
#include "function.h"
#include <map>
#include <queue>
#include <sstream>
using namespace std;
//以下为重要的变量
U8* sendbuf = NULL;        //用来组织发送数据的缓存，大小为MAX_BUFFER_SIZE,可以在这个基础上扩充设计，形成适合的结构，例程中没有使用，只是提醒一下
int printCount = 0; //打印控制
int spin = 0;  //打印动态信息控制

//------华丽的分割线，一些统计用的全局变量------------
int iSndTotal = 0;  //发送数据总量
int iSndTotalCount = 0; //发送数据总次数
int iSndErrorCount = 0;  //发送错误次数
int iRcvForward = 0;     //转发数据总量
int iRcvForwardCount = 0; //转发数据总次数
int iRcvToUpper = 0;      //从低层递交高层数据总量
int iRcvToUpperCount = 0;  //从低层递交高层数据总次数
int iRcvUnknownCount = 0;  //收到不明来源数据总次数
bool isTimerStart = false;
bool isTimerStart1 = false;
bool isTimerStart2 = false;
int tickTack = 0; //计时器
int tickTack1 = 0;
int tickTack2 = 0;
int lengthSave = 0; //缓存长度
int preSequence = 0; //记录上一个序列
int frameHeadLen = 8; //帧头长度
int frameBackLen = 8; //帧尾长度
int macLen = 8; //mac地址长度
int frameNumLen = 8; //序号长度
int crcLen = 8; //crc长度
//接收缓存计数
int seqCount = 1;
//接收方接收正确ack数
int sendSeq = 1;
int isRightSend[2000] = { 0 };
//重传队列
queue<int> resendQueue;
//发送缓存
map<int, U8*> saveBuf;
//交换机缓存
map<int, U8*> switchBuf;
//存储mac和接口
map<int, int> mapMac;
map<int, int>::iterator iter;
//接收窗口
map<int, U8*> recvBuf;

//打印统计信息
void print_statistics();
void menu();
//***************重要函数提醒******************************
//名称：InitFunction
//功能：初始化功能面，由main函数在读完配置文件，正式进入驱动机制前调用
//输入：
//输出：
void InitFunction(CCfgFileParms& cfgParms)
{
	sendbuf = (char*)malloc(MAX_BUFFER_SIZE);
	if (sendbuf == NULL ) {
		cout << "内存不够" << endl;
		//这个，计算机也太，退出吧
		exit(0);
	}
	return;
}
//***************重要函数提醒******************************
//名称：EndFunction
//功能：结束功能面，由main函数在收到exit命令，整个程序退出前调用
//输入：
//输出：
void EndFunction()
{
	if(sendbuf != NULL)
		free(sendbuf);
	return;
}

//字符串转整型
int string2int(string a) {
	stringstream stream;
	stream << a;
	int b;
	stream >> b;
	return b;
}

//获取数据长度
int getLen(U8* buf) {
	int i = 0;
	while (buf[i] == 0 || buf[i] == 1)
		i++;
	return i;
}
//获取mac地址
int getMac(U8* buf) {
	return buf[9] * 4 + buf[10] * 2 + buf[11];
}
//交换机发送至物理层
void send2low(U8* buf, int len, int target) {
	U8* bufSend = NULL;
	int iSndRetval = 0;
	int switchSource = getMac(buf);
	if (lowerMode[switchSource] == lowerMode[target]) {
		//接口0和1的数据格式相同，直接转发
		iSndRetval = SendtoLower(buf, len, target);
		if (lowerMode[switchSource] == 1) {
			iSndRetval = iSndRetval * 8;//如果接口格式为bit数组，统一换算成位，完成统计
		}
	}
	else {
		//接口0与接口1的数据格式不同，需要转换后，再发送
		if (lowerMode[switchSource] == 1) {
			//从接口0到接口1，接口0是字节数组，接口1是比特数组，需要扩大8倍转换
			bufSend = (U8*)malloc(len * 8);
			if (bufSend == NULL) {
				cout << "内存空间不够，导致数据没有被处理" << endl;
				return;
			}
			//byte to bit
			iSndRetval = ByteArrayToBitArray(bufSend, len * 8, buf, len);
			iSndRetval = SendtoLower(bufSend, iSndRetval, target);
			//iSndRetval = SendtoLower(bufSend, iSndRetval, 2);
		}
		else {
			//从接口0到接口1，接口0是比特数组，接口1是字节数组，需要缩小八分之一转换
			bufSend = (U8*)malloc(len / 8 + 1);
			if (bufSend == NULL) {
				cout << "内存空间不够，导致数据没有被处理" << endl;
				return;
			}
			//bit to byte
			iSndRetval = BitArrayToByteArray(buf, len, bufSend, len / 8 + 1);
			iSndRetval = SendtoLower(bufSend, iSndRetval, target);
			iSndRetval = iSndRetval * 8;//换算成位，做统计
		}
	}
	//统计
	if (iSndRetval <= 0) {
		iSndErrorCount++;
	}
	else {
		iRcvToUpper += iSndRetval;
		iRcvToUpperCount++;
	}
	free(bufSend);
}

//广播发送给自身以外的每一个实体
void broadcast(U8* buf, int len) {
	int i = 0;
	printf("lowerNum: %d\n", lowerNumber);
	for (int times = 0; times < lowerNumber; times++) {
		if (times != mapMac[getMac(buf)]) {
			send2low(buf, len, times);
		}
	}
}

//打印地址表
void printChart() {
	printf("**************************mac port chart**************************\n");
	for (iter = mapMac.begin(); iter != mapMac.end(); iter++)
		cout << "	mac(enity id): " << iter->first << "------------------port: " << iter->second << endl;
	printf("******************************************************************\n");
}

//单播
void unicast(U8* buf, int len) {
	U8* bufSend = NULL;
	int iSndRetval = 0;
	int switchSource = 0;
	int switchTarget = 0;
	switchSource = getMac(buf);
	switchTarget = buf[12] * 4 + buf[13] * 2 + buf[14];
	printf("target: %d\n", switchTarget);
	//如果mac已知
	if (mapMac.count(switchTarget)) {
		printChart();
		printf("KNOWN MAC\n");
		send2low(buf, len, mapMac[switchTarget]);
	}
	else {
		printf("UNKNOWN MAC\n");
		//广播
		broadcast(buf, len);
		if (!(mapMac.count(switchTarget))) {
			mapMac[switchTarget] = 0;
		}
	}
}
//打印比特流
void printBuf(U8* buf) {
	printf("\n|||||||||||||||printbuf||||||||||||||||\n");
	for (int i = 0; i < getLen(buf); i++)
		printf("%d ", buf[i]);
	printf("\n|||||||||||||||printbuf||||||||||||||||\n");
}
//打印缓存
void printBackUp(int sequence) {
	printf("\n**************************Back-----Up*****************************\n");
	for (int i = 0; i < getLen(saveBuf[sequence]); i++)
		printf("%d ", saveBuf[sequence][i]);
	printf("\n******************************************************************\n");
}
//***************重要函数提醒******************************
//名称：TimeOut
//功能：本函数被调用时，意味着sBasicTimer中设置的超时时间到了，
//      函数内容可以全部替换为设计者自己的想法
//      例程中实现了几个同时进行功能，供参考
//      1)根据iWorkMode工作模式，判断是否将键盘输入的数据发送，
//        因为scanf会阻塞，导致计时器在等待键盘的时候完全失效，所以使用_kbhit()无阻塞、不间断地在计时的控制下判断键盘状态，这个点Get到没？
//      2)不断刷新打印各种统计值，通过打印控制符的控制，可以始终保持在同一行打印，Get？
//输入：时间到了就触发，只能通过全局变量供给输入
//输出：这就是个不断努力干活的老实孩子
int seq = 1;
int resendSeq = 0;
int isResend[200] = { -1 };
int recvSeq = 1;
void TimeOut() {
	int switchSource = 0;
	int switchTarget = 0;
	int sequence = 0;
	int i = 0;
	//交换机发送
	if (isTimerStart == true) {
		//tickTack++;
		if (tickTack % 1 == 0) {
			//广播or单播
			if ((switchBuf.count(seq))) {
				if (switchBuf[seq][frameHeadLen]) {
					//广播
					printf("\nBROAD\n");
					if (switchBuf.count(seq)) {
						broadcast(switchBuf[seq], getLen(switchBuf[seq]));
					}
				}
				else {
					//单播
					//获取目的mac
					printf("\nUNICAST\n");
					unicast(switchBuf[seq], getLen(switchBuf[seq]));
				}
				preSequence = sequence;
				seq++;
			}
		}
	}
	//重传
	if (isTimerStart1 == true) {
		if (tickTack1 % 1 == 0) {
			if (!(resendQueue.empty())) {
				printf("\n//////////////resend through switch/////////////\n");
				sequence = resendQueue.front();
				resendQueue.pop();
				if (switchBuf[sequence][frameHeadLen]) {
					//广播
					printf("\nBROAD\n");
					if (switchBuf.count(sequence)) {
						switchSource = switchBuf[sequence][9] * 4 + switchBuf[sequence][10] * 2 + switchBuf[sequence][11];
						switchTarget = switchBuf[sequence][12] * 4 + switchBuf[sequence][13] * 2 + switchBuf[sequence][14];
						broadcast(switchBuf[sequence], getLen(switchBuf[sequence]));
					}
				}
				else {
					//单播
					//获取目的mac
					printf("\nUNICAST\n");
					unicast(switchBuf[sequence], getLen(switchBuf[sequence]));
				}
				//isTimerStart = false;
			}
		}
	}
	//设置超时重传
	if (isTimerStart2 == true) {
		tickTack2++;
		if (saveBuf.count(sendSeq)) {
			if (tickTack2 == 5) {
				printf("\n////////////Time Out////////////\n");
				printf("Resend sendSeq: %d\n", sendSeq);
				int a = sendSeq;
				SendtoLower(saveBuf[a], getLen(saveBuf[a]), 0);
				//SendtoLower(saveBuf[a - 1], getLen(saveBuf[a - 1]), 0);
				//SendtoLower(saveBuf[a + 1], getLen(saveBuf[a + 1]), 0);
				//printBackUp(a);
			}
		}
	}
	if (_kbhit()) {
		//键盘有动作，进入菜单模式
		menu();
	}

	print_statistics();
}
//------------华丽的分割线，以下是数据的收发,--------------------------------------------

//长1添0：帧头长度，总数据， 除首位数据长度，新生成数据
int addZero(int head, U8* sendBitArray, int len, U8* sendBitArray1) {
	int iSndRetval = 0; //输出长度
	int oneCount = 0; //对1计数
	int i;
	int addZeroLength = 0; //初始化计数器
	sendBitArray1[head] = sendBitArray[head];
	//从数据的第二位开始
	//上界的确定：由于字符串自带'\0'，-8去除终止符（X）
	for (i = head + 1, addZeroLength = head + 1; i < len + head; i++, addZeroLength++) {
		//printf("\naddZeroLength=%d\n", oneCount);
		//printf("buf[i]: %d, buf[i-1]: %d\n", sendBitArray[i], sendBitArray[i - 1]);
		sendBitArray1[addZeroLength] = sendBitArray[i];
		//逐个判断是不是出现了连续5个1
		if (sendBitArray[i] == 1 && sendBitArray[i - 1] == 1) {
			oneCount++;
		}
		else {
			oneCount = 0;
		}
		if (oneCount == 4) {
			//包括bitArray[i]有5个连续的1
			addZeroLength++;
			sendBitArray1[addZeroLength] = 0;  //插入1个零
			oneCount = 0;
		}
	}
	return addZeroLength;
}

//同步帧：数据，总长
void frameAlignment(U8* sendBitArray, int dataLen) {
	//首
	int i;
	sendBitArray[0] = 0;
	sendBitArray[frameHeadLen - 1] = 0;
	for (i = 1; i < frameHeadLen - 1; i++) {
		sendBitArray[i] = 1;
	}
	//尾
	sendBitArray[dataLen - 8] = 0;
	sendBitArray[dataLen - 1] = 0;
	for (i = dataLen - 7; i < dataLen - 1; i++) {
		sendBitArray[i] = 1;
	}
}

//初始化mac表
void macInit(U8* sendBitArray, U8* mac) {
	U8* mac8 = (U8*)malloc(8);
	ByteArrayToBitArray(mac8, 8, mac, 1);
	for (int i = 8; i < 16; i++)
		sendBitArray[i] = mac8[i - 8];
	free(mac8);
}

//序列计数
void sequenceCount(U8* sendBitArray) {
	//count num
	int i = frameHeadLen + macLen + frameNumLen - 1;
	int sendCount = 0;
	for (int j = i; j >= frameHeadLen + macLen; j--)
		sendBitArray[j] = 0;
	sendCount = iSndTotalCount + 1;
	while (sendCount) {
		if (sendCount % 2) {
			sendBitArray[i] = 1;
		}
		else {
			sendBitArray[i] = 0;
		}
		sendCount /= 2;
		i--;
	}
}

//添加crc
void crc(U8* sendBitArray, int dataLen){
	//CRC
	//generate
	int zero_count = 0; //对0计数
	int i;
	U8* sendBitArray0 = NULL;
	U8 generateCode[9] = { 1, 0, 0, 0, 0, 0, 1, 1, 1 };
	zero_count = 0;
	sendBitArray0 = (U8*)malloc(dataLen + 8);
	//copy
	//添0
	for (i = dataLen - (frameHeadLen + macLen + frameNumLen); i < dataLen - (frameHeadLen + macLen + frameNumLen) + 8; i++) {
		sendBitArray0[i] = 0;
	}
	//原数据
	for (i = frameHeadLen + macLen + frameNumLen; i < dataLen; i++) {
		sendBitArray0[i - (frameHeadLen + macLen + frameNumLen)] = sendBitArray[i];
	}
	//模二除
	while (1) {
		while (!sendBitArray0[0] && zero_count < dataLen - (frameHeadLen + macLen + frameNumLen)) {
			for (i = 0; i < dataLen - (frameHeadLen + macLen + frameNumLen) + 8 - zero_count - 1; i++) {
				sendBitArray0[i] = sendBitArray0[i + 1];
			}
			zero_count++;
		}
		if (zero_count == dataLen - (frameHeadLen + macLen + frameNumLen) && !sendBitArray0[0])
			break;
		for (i = 0; i < 9; i++) {
			if (sendBitArray0[i] == generateCode[i])
				sendBitArray0[i] = 0;
			else
			{
				sendBitArray0[i] = 1;
			}
		}
	}
	//最终结果在[0]~[7]
	for (i = dataLen; i < dataLen + 8; i++) {
		sendBitArray[i] = sendBitArray0[i - dataLen];
	}
	free(sendBitArray0);
}

//获取序列
int getSequence(U8* buft) {
	int sequence = 0, i = 0;
	for (i = 16; i < 24; i++) {
		//printf("seq buft[i]: %d\n", buft[i]);
		sequence += int(pow(2, (23 - i))) * buft[i];
	}
	return sequence;
}

//缓存
void backUp(U8* sendBitArray, int dataLen) {
	int i;
	int sequence = getSequence(sendBitArray);
	lengthSave = dataLen + crcLen + frameBackLen;
	saveBuf[sequence] = (U8*)malloc(lengthSave);
	for (i = 0 ; i < lengthSave; i++)
		saveBuf[sequence][i] = sendBitArray[i];
	//全局变量缓存
}
//***************重要函数提醒******************************
//名称：RecvfromUpper
//功能：本函数被调用时，意味着收到一份高层下发的数据
//      函数内容全部可以替换成设计者自己的
//      例程功能介绍
//         1)通过低层的数据格式参数lowerMode，判断要不要将数据转换成bit流数组发送，发送只发给低层接口0，
//           因为没有任何可供参考的策略，讲道理是应该根据目的地址在多个接口中选择转发的。
//         2)判断iWorkMode，看看是不是需要将发送的数据内容都打印，调试时可以，正式运行时不建议将内容全部打印。
//输入：U8 * buf,高层传进来的数据， int len，数据长度，单位字节
//输出：
void RecvfromUpper(U8* buf, int len){	
	int iSndRetval; //输出长度
	U8* bufSend = NULL;
	int i = 0, j = 0;
	//截取最后的地址信息，数据在buf1
	U8* saveMac = (U8*)malloc(1);
	saveMac[0] = buf[len - 1];
	len--;
	U8* buf1 = (U8*)malloc(len);
	for (i = 0; i < len; i++) {
		buf1[i] = buf[i];
		//printf("%d ", buf[i]);
	}

	//是高层数据，只从接口0发出去,高层接口默认都是字节流数据格式
	if (lowerMode[0] == 0) {
		//接口0的模式为bit数组，先转换成bit数组，放到bufSend里
		bufSend = (U8*)malloc(len * 8 + frameHeadLen + macLen + frameNumLen + crcLen + frameBackLen);
		//+xx: 将len扩大8倍并置于数据的首地址
		iSndRetval = ByteArrayToBitArray(bufSend + frameHeadLen + macLen + frameNumLen, len * 8, buf1, len);
		//mac
		macInit(bufSend, saveMac);
		//添加序列
		sequenceCount(bufSend);
		//crc
		crc(bufSend, iSndRetval + 24);
		//*16: 足够空间添0；+xx： 封装空间
		U8* sendBitArray1 = (U8*)malloc(len * 16 + 40);
		//长1添0
		int addLen = addZero(frameHeadLen, bufSend, len * 8 + 24, sendBitArray1);
		int addLen1 = addLen + 8;
		//帧同步
		frameAlignment(sendBitArray1, addLen1);
		//printBuf(sendBitArray1);
		//发送
		iSndRetval = SendtoLower(sendBitArray1, addLen1, 0); //参数依次为数据缓冲，长度，接口号
		//printBuf(sendBitArray1);
		//发送后开始计时，超时则重传
		isTimerStart2 = true;
		tickTack2 = 0;
		backUp(sendBitArray1, addLen1);
		//printf("\ndataLen + 16: %d\n", dataLen + 16);
		//print_data_bit(sendBitArray1, addLen1, 0);
	}
	else {
		//下层是字节数组接口，可直接发送
		iSndRetval = SendtoLower(buf1, len, 0);
		iSndRetval = iSndRetval * 8;//换算成位
	}
	//统计
	if (iSndRetval <= 0) {
		iSndErrorCount++;
	}
	else {
		iSndTotal += iSndRetval;
		iSndTotalCount++;
	}
	//printf("\n收到上层数据 %d 位，发送到接口0\n", retval * 8);
	//打印
	switch (iWorkMode % 10) {
	case 1:
		cout << endl << "高层要求向接口 " << 0 << " 发送数据：" << endl;
		print_data_bit(buf1, len, 1);
		break;
	case 2:
		cout << endl << "高层要求向接口 " << 0 << " 发送数据：" << endl;
		print_data_byte(buf1, len, 1);
		break;
	case 0:
		break;
	}
	free(buf1);
}

//送往物理层
void send2phy(U8* buf, int len, int ifNo) {
	U8* buf2 = (U8*)malloc(2 * len);
	int leng = addZero(8, buf, len - 16, buf2);
	frameAlignment(buf2, leng + 8);
	SendtoLower(buf2, leng + 8, ifNo);
	free(buf2);
}

//抽取：原长，原数据，抽取后的数据；返回数据长度
int extract(int len, U8* buf, U8* buft) {
	int arrLength = 0; //接收处理后的长度
	int flag1 = 0;
	int flag2 = 0;//位标1 2
	int i, j;
	for (i = 0; i < len * 8; i++) {
		if (buf[i] == 0 &&
			buf[i + 1] == 1 &&
			buf[i + 2] == 1 &&
			buf[i + 3] == 1 &&
			buf[i + 4] == 1 &&
			buf[i + 5] == 1 &&
			buf[i + 6] == 1 &&
			buf[i + 7] == 0) {
			flag1 = i;
			break;
		}
	}
	for (j = i + 1; j < len * 8; j++) {
		if (buf[j] == 0 &&
			buf[j + 1] == 1 &&
			buf[j + 2] == 1 &&
			buf[j + 3] == 1 &&
			buf[j + 4] == 1 &&
			buf[j + 5] == 1 &&
			buf[j + 6] == 1 &&
			buf[j + 7] == 0) {
			flag2 = j + 8;
			break;
		}
	}
	arrLength = flag2 - flag1;
	//复制数组,cut
	for (i = 0; i < arrLength; i++) {
		buft[i] = buf[i + flag1];
	}
	return arrLength;
}

//检验crc
void checkCrc(U8* buft, U8* sendBitArray0, int arrLength) {
	int zero_count = 0;
	int i;
	printf("\nChecking CRC...");
	//check crc
	U8 generateCode[9] = { 1, 0, 0, 0, 0, 0, 1, 1, 1 };
	zero_count = 0;
	//copy
	//数据+crc
	for (i = frameHeadLen + macLen + frameBackLen; i < arrLength - frameBackLen; i++) {
		sendBitArray0[i - (frameHeadLen + macLen + frameBackLen)] = buft[i];
	}
	//模二除
	while (1) {
		while (!sendBitArray0[0] && zero_count < arrLength - (frameHeadLen + macLen + frameNumLen + crcLen + frameBackLen)) {
			for (i = 0; i < arrLength - (frameHeadLen + macLen + frameNumLen + crcLen + frameBackLen) + 8 - zero_count - 1; i++) {
				sendBitArray0[i] = sendBitArray0[i + 1];
			}
			zero_count++;
		}
		if (zero_count == arrLength - (frameHeadLen + macLen + frameNumLen + crcLen + frameBackLen) && !sendBitArray0[0])
			break;
		for (i = 0; i < 9; i++) {
			if (sendBitArray0[i] == generateCode[i])
				sendBitArray0[i] = 0;
			else
			{
				sendBitArray0[i] = 1;
			}
		}
	}
}


//更新mac表
void updateMac(U8* buf, int ifNo) {
	//更新mac
	int id = getMac(buf);
	mapMac[id] = ifNo;
}

//送确认帧
void sendAck(U8* ack, bool flag, U8* buf, int ifNo) {
	int i;	
	int id;
	for (i = 0; i < 32; i++)
		ack[i] = 0;
	//判断正确与否
	if (!(flag))
		ack[15] = 0;
	else
		ack[15] = 1;
	//封装
	for (i = 1; i < 7; i++)
		ack[i] = 1;
	for (i = 25; i < 31; i++)
		ack[i] = 1;
	id = string2int(strDevID) - 1;
	i = 11;
	while (id) {
		ack[i] = id % 2;
		id /= 2;
		i--;
	}
	updateMac(buf, ifNo);
	ack[12] = buf[9];
	ack[13] = buf[10];
	ack[14] = buf[11];
	for (i = 16; i < 24; i++) {
		ack[i] = buf[i];
	}
	printf("have judged and send back ack\n");
	for (i = 0; i < 32; i++) {
		printf("%d ", ack[i]);
	}
	printf("Sending ack\n\n");
	send2phy(ack, 32, ifNo);
}

//还原长1
void return2one(int head, int back, U8* buft0, U8* buft, int& arrLength) {
	int i, j;
	int flag = 0;
	int oneCount = 0;
	buft0[head] = buft[head];
	for (i = head + 1, j = head + 1; i < arrLength - back; i++) {
		if (flag) {
			if (buft[i] == 0) {
				i++;
				buft0[j] = buft[i];
				if (i != arrLength - back)
					j++;
			}
			flag = 0;
			continue;
		}
		buft0[j] = buft[i];
		j++;
		if (buft[i] == 1 && buft[i - 1] == 1) {
			oneCount++;
		}
		else {
			oneCount = 0;
		}
		if (oneCount == 4) {
			flag = 1;
			oneCount = 0;
		}
	}
	for (i = 0; i < head; i++) {
		buft0[i] = buft[i];
	}
	int m;
	m = arrLength - 8;
	for (i = j; i < j + 8; i++) {
		buft0[i] = buft[m];
		m++;
	}
	arrLength = j + 8;
}

//重传
void resend(int sequence, int ifNo) {
	printf("RESEND------------len: %d----------sequence: %d\n", getLen(saveBuf[sequence]), sequence);
	//printBackUp(sequence);
	SendtoLower(saveBuf[sequence], getLen(saveBuf[sequence]), 0);
	tickTack2 = 1;
}

//***************重要函数提醒******************************
//名称：RecvfromLower
//功能：本函数被调用时，意味着得到一份从低层实体递交上来的数据
//      函数内容全部可以替换成设计者想要的样子
//      例程功能介绍：
//          1)例程实现了一个简单粗暴不讲道理的策略，所有从接口0送上来的数据都直接转发到接口1，而接口1的数据上交给高层，就是这么任性
//          2)转发和上交前，判断收进来的格式和要发送出去的格式是否相同，否则，在bite流数组和字节流数组之间实现转换
//            注意这些判断并不是来自数据本身的特征，而是来自配置文件，所以配置文件的参数写错了，判断也就会失误
//          3)根据iWorkMode，判断是否需要把数据内容打印
//输入：U8 * buf,低层递交上来的数据， int len，数据长度，单位字节，int ifNo ，低层实体号码，用来区分是哪个低层
void RecvfromLower(U8* buf, int len, int ifNo) {
	int i;
	int longOneCount = 0;
	U8* ack = (U8*)malloc(32);
	U8* buft = NULL;
	bool flag = true;
	bool flag1 = true;
	int iSndRetval = 0;
	int iSndRetval2 = 0;
	U8* bufSend2 = NULL;
	U8* sendBitArray0 = NULL;
	int sequence = 0;
	buft = (U8*)malloc(len);
	int arrLength = extract(len, buf, buft);
	//print_data_bit(buft, arrLength, 0);
	if (arrLength < 32)
		free(buft);
	else {
		U8* buft0 = (U8*)malloc(arrLength);
		printf("arrlen: %d\n", arrLength);
		//1还原
		return2one(8, 8, buft0, buft, arrLength);
		sequence = getSequence(buft0);
		printf("\n................sequen: %d..............\n", sequence);
		printf("arrlen: %d\n", arrLength);
		//print_data_bit(buft, arrLength, 0);
		//if (sequence == 35) {
		//	for (i = 0; i < 800; i++)
		//		printf("%d ", buft0[i]);
		//}
		//如果是交换机
		if (lowerNumber > 1) {
			//若为ack，学习，直接发送
			if (arrLength == 32) {
				updateMac(buft0, ifNo);
				printChart();
				printf("\nSwitch Receive ACK!*********************************\n");
				for (i = 0; i < 32; i++)
					printf("%d ", buft0[i]);
				printf("\nSwitch Sending ACK!*********************************\n");
				send2phy(buft0, 32, mapMac[buft0[12] * 4 + buft0[13] * 2 + buft0[14]]);
			}
			else {
				printf("Save and Send\n");
				//若无源地址mac，学习
				if (!(mapMac.count(getMac(buft0)))) {
					updateMac(buft0, ifNo);
					printChart();
				}
				//若不同, 重新学习（省略）
				//存储并转发
				isResend[sequence] += 1;
				switchBuf[sequence] = (U8*)malloc(getLen(buft0));
				int newLen = addZero(8, buft0, arrLength - 16, switchBuf[sequence]);
				frameAlignment(switchBuf[sequence], newLen + 8);
				if (isResend[sequence] >= 2) {
					//将需要重传的序列压入队列
					if (!(recvBuf.count(sequence))) {
						resendQueue.push(sequence);
						isResend[sequence] = 1;
					}
				}
				tickTack = 0;
				tickTack1 = 0;
				isTimerStart = true;
				isTimerStart1 = true;
			}
		}
		//如果不是交换机且帧长度大于32，说明是数据报文，进行各种处理
		else if (arrLength > 32) {
			//若当前重传序列已正确接收，则丢弃
			if (recvBuf.count(sequence)) {
				printf("ALREADY EXIST, ABANDON SEQUENCE: %d\n", sequence);
			}
			else {
				//抽取crc
				sendBitArray0 = (U8*)malloc(arrLength);
				//check crc
				flag = true;
				checkCrc(buft0, sendBitArray0, arrLength);
				for (i = 0; i < 8; i++) {
					if (sendBitArray0[i]) {
						printf("\nError!\n");
						iSndErrorCount++;
						flag = false;
						break;
					}
				}
				free(sendBitArray0);
				if (flag) {
					printf("\nRight!\n");
				}
				//ack
				sendAck(ack, flag, buft0, ifNo);
				//若为正确帧
				if (flag) {
					//接收窗口暂存
					recvBuf[sequence] = (U8*)malloc(arrLength);
					for (i = 0; i < arrLength; i++) {
						recvBuf[sequence][i] = buft0[i];
					}
					if (arrLength < 500) {
						bufSend2 = (U8*)malloc(getLen(recvBuf[sequence]) / 8);
						iSndRetval2 = BitArrayToByteArray(recvBuf[sequence], getLen(recvBuf[sequence]), bufSend2, getLen(recvBuf[sequence]) / 8);
						iSndRetval2 = SendtoUpper(bufSend2, iSndRetval2);
						if (iSndRetval2 <= 0) {
							iSndErrorCount++;
						}
						else {
							iRcvToUpper += iSndRetval2;
							iRcvToUpperCount++;
						}
					}
					else {
						while (recvBuf.count(seqCount)) {
							printf("==========================\n");
							printf("%d\n", seqCount);
							printf("arrLen: %d\n", arrLength);
							if (lowerMode[ifNo] == 0) {
								//如果接口0是比特数组格式，高层默认是字节数组，先转换成字节数组，再向上递交
								bufSend2 = (U8*)malloc(getLen(recvBuf[seqCount]) / 8);
								iSndRetval2 = BitArrayToByteArray(recvBuf[seqCount], getLen(recvBuf[seqCount]), bufSend2, getLen(recvBuf[seqCount]) / 8);
								iSndRetval2 = SendtoUpper(bufSend2, iSndRetval2);
							}
							else {
								//低层是字节数组接口，可直接递交
								iSndRetval2 = SendtoUpper(recvBuf[seqCount], getLen(recvBuf[seqCount]));
								iSndRetval2 = iSndRetval2 * 8;
							}
							if (iSndRetval2 <= 0) {
								iSndErrorCount++;
							}
							else {
								iRcvToUpper += iSndRetval2;
								iRcvToUpperCount++;
							}
							seqCount++;
						}
					}
				}
			}
		}
		//如果长度为32，说明是确认帧
		else if (arrLength == 32) {
			printf("\nsend Receive ACK!*********************************\n");
			for (i = 0; i < 32; i++)
				printf("%d ", buft0[i]);
			printf("\nsend Receive ACK!*********************************\n");
			if (!(buft0[15])) {
				resend(sequence, ifNo);
			}
			else {
				isRightSend[sequence] = 1;
				if (sequence == sendSeq) {
					//标记已接收
					printf("Send Receive Right ack, sequence: %d, sendSeq: %d\n", sequence, sendSeq);
					while (isRightSend[sendSeq]) {
						printf("sendSeq: %d\n", sendSeq);
						sendSeq++;
					}
					isTimerStart2 = false;
				}
				printf("No Need Resend\n");
			}
		}
		free(buft0);
	}
	
	//打印
	switch (iWorkMode % 10) {
	case 1:
		cout << endl << "接收接口 " << ifNo << " 数据：" << endl;
		print_data_bit(buf, len, lowerMode[ifNo]);
		break;
	case 2:
		cout << endl << "接收接口 " << ifNo << " 数据：" << endl;
		print_data_byte(buf, len, lowerMode[ifNo]);
		break;
	case 0:
		break;
	}
}

void print_statistics()
{
	if (printCount % 10 == 0) {
		switch (spin) {
		case 1:
			printf("\r-");
			break;
		case 2:
			printf("\r\\");
			break;
		case 3:
			printf("\r|");
			break;
		case 4:
			printf("\r/");
			spin = 0;
			break;
		}
		cout << "共转发 "<< iRcvForward<< " 位，"<< iRcvForwardCount<<" 次，"<<"递交 "<< iRcvToUpper<<" 位，"<< iRcvToUpperCount<<" 次,"<<"发送 "<< iSndTotal <<" 位，"<< iSndTotalCount<<" 次，"<< "发送不成功 "<< iSndErrorCount<<" 次,""收到不明来源 "<< iRcvUnknownCount<<" 次。";
		spin++;
	}
}
void menu()
{
	int selection;
	unsigned short port;
	int iSndRetval;
	char kbBuf[100];
	int len;
	U8* bufSend;
	//发送|打印：[发送控制（0，等待键盘输入；1，自动）][打印控制（0，仅定期打印统计信息；1，按bit流打印数据，2按字节流打印数据]
	cout << endl << endl << "设备号:" << strDevID << ",    层次:" << strLayer << ",    实体号:" << strEntity;
	cout << endl << "1-启动自动发送(无效);" << endl << "2-停止自动发送（无效）; " << endl << "3-从键盘输入发送; ";
	cout << endl << "4-仅打印统计信息; " << endl << "5-按比特流打印数据内容;" << endl << "6-按字节流打印数据内容;";
	cout << endl << "0-取消" << endl << "请输入数字选择命令：";
	cin >> selection;
	switch (selection) {
	case 0:

		break;
	case 1:
		iWorkMode = 10 + iWorkMode % 10;
		break;
	case 2:
		iWorkMode = iWorkMode % 10;
		break;
	case 3:
		cout << "输入字符串(,不超过100字符)：";
		cin >> kbBuf;
		cout << "输入低层接口号：";
		cin >> port;

		len = (int)strlen(kbBuf) + 1; //字符串最后有个结束符
		if (port >= lowerNumber) {
			cout << "没有这个接口" << endl;
			return;
		}
		if (lowerMode[port] == 0) {
			//下层接口是比特流数组,需要一片新的缓冲来转换格式
			bufSend = (U8*)malloc(len * 8);

			iSndRetval = ByteArrayToBitArray(bufSend, len * 8, kbBuf, len);
			iSndRetval = SendtoLower(bufSend, iSndRetval, port);
		}
		else {
			//下层接口是字节数组，直接发送
			iSndRetval = SendtoLower(kbBuf, len, port);
			iSndRetval = iSndRetval * 8; //换算成位
		}
		//发送统计
		if (iSndRetval > 0) {
			iSndTotalCount++;
			iSndTotal += iSndRetval;
		}
		else {
			iSndErrorCount++;
		}
		//看要不要打印数据
		cout << endl << "向接口 " << port << " 发送数据：" << endl;
		switch (iWorkMode % 10) {
		case 1:
			print_data_bit(kbBuf, len, 1);
			break;
		case 2:
			print_data_byte(kbBuf, len, 1);
			break;
		case 0:
			break;
		}
		break;
	case 4:
		iWorkMode = (iWorkMode / 10) * 10 + 0;
		break;
	case 5:
		iWorkMode = (iWorkMode / 10) * 10 + 1;
		break;
	case 6:
		iWorkMode = (iWorkMode / 10) * 10 + 2;
		break;
	}

}
//弯路：函数封装；帧规划；协议选择；mac规划；交换机未纠错；不使用所有检错；最后添0，最早去0