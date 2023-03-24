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
//����Ϊ��Ҫ�ı���
U8* sendbuf = NULL;        //������֯�������ݵĻ��棬��СΪMAX_BUFFER_SIZE,���������������������ƣ��γ��ʺϵĽṹ��������û��ʹ�ã�ֻ������һ��
int printCount = 0; //��ӡ����
int spin = 0;  //��ӡ��̬��Ϣ����

//------�����ķָ��ߣ�һЩͳ���õ�ȫ�ֱ���------------
int iSndTotal = 0;  //������������
int iSndTotalCount = 0; //���������ܴ���
int iSndErrorCount = 0;  //���ʹ������
int iRcvForward = 0;     //ת����������
int iRcvForwardCount = 0; //ת�������ܴ���
int iRcvToUpper = 0;      //�ӵͲ�ݽ��߲���������
int iRcvToUpperCount = 0;  //�ӵͲ�ݽ��߲������ܴ���
int iRcvUnknownCount = 0;  //�յ�������Դ�����ܴ���
bool isTimerStart = false;
bool isTimerStart1 = false;
bool isTimerStart2 = false;
int tickTack = 0; //��ʱ��
int tickTack1 = 0;
int tickTack2 = 0;
int lengthSave = 0; //���泤��
int preSequence = 0; //��¼��һ������
int frameHeadLen = 8; //֡ͷ����
int frameBackLen = 8; //֡β����
int macLen = 8; //mac��ַ����
int frameNumLen = 8; //��ų���
int crcLen = 8; //crc����
//���ջ������
int seqCount = 1;
//���շ�������ȷack��
int sendSeq = 1;
int isRightSend[2000] = { 0 };
//�ش�����
queue<int> resendQueue;
//���ͻ���
map<int, U8*> saveBuf;
//����������
map<int, U8*> switchBuf;
//�洢mac�ͽӿ�
map<int, int> mapMac;
map<int, int>::iterator iter;
//���մ���
map<int, U8*> recvBuf;

//��ӡͳ����Ϣ
void print_statistics();
void menu();
//***************��Ҫ��������******************************
//���ƣ�InitFunction
//���ܣ���ʼ�������棬��main�����ڶ��������ļ�����ʽ������������ǰ����
//���룺
//�����
void InitFunction(CCfgFileParms& cfgParms)
{
	sendbuf = (char*)malloc(MAX_BUFFER_SIZE);
	if (sendbuf == NULL ) {
		cout << "�ڴ治��" << endl;
		//����������Ҳ̫���˳���
		exit(0);
	}
	return;
}
//***************��Ҫ��������******************************
//���ƣ�EndFunction
//���ܣ����������棬��main�������յ�exit������������˳�ǰ����
//���룺
//�����
void EndFunction()
{
	if(sendbuf != NULL)
		free(sendbuf);
	return;
}

//�ַ���ת����
int string2int(string a) {
	stringstream stream;
	stream << a;
	int b;
	stream >> b;
	return b;
}

//��ȡ���ݳ���
int getLen(U8* buf) {
	int i = 0;
	while (buf[i] == 0 || buf[i] == 1)
		i++;
	return i;
}
//��ȡmac��ַ
int getMac(U8* buf) {
	return buf[9] * 4 + buf[10] * 2 + buf[11];
}
//�����������������
void send2low(U8* buf, int len, int target) {
	U8* bufSend = NULL;
	int iSndRetval = 0;
	int switchSource = getMac(buf);
	if (lowerMode[switchSource] == lowerMode[target]) {
		//�ӿ�0��1�����ݸ�ʽ��ͬ��ֱ��ת��
		iSndRetval = SendtoLower(buf, len, target);
		if (lowerMode[switchSource] == 1) {
			iSndRetval = iSndRetval * 8;//����ӿڸ�ʽΪbit���飬ͳһ�����λ�����ͳ��
		}
	}
	else {
		//�ӿ�0��ӿ�1�����ݸ�ʽ��ͬ����Ҫת�����ٷ���
		if (lowerMode[switchSource] == 1) {
			//�ӽӿ�0���ӿ�1���ӿ�0���ֽ����飬�ӿ�1�Ǳ������飬��Ҫ����8��ת��
			bufSend = (U8*)malloc(len * 8);
			if (bufSend == NULL) {
				cout << "�ڴ�ռ䲻������������û�б�����" << endl;
				return;
			}
			//byte to bit
			iSndRetval = ByteArrayToBitArray(bufSend, len * 8, buf, len);
			iSndRetval = SendtoLower(bufSend, iSndRetval, target);
			//iSndRetval = SendtoLower(bufSend, iSndRetval, 2);
		}
		else {
			//�ӽӿ�0���ӿ�1���ӿ�0�Ǳ������飬�ӿ�1���ֽ����飬��Ҫ��С�˷�֮һת��
			bufSend = (U8*)malloc(len / 8 + 1);
			if (bufSend == NULL) {
				cout << "�ڴ�ռ䲻������������û�б�����" << endl;
				return;
			}
			//bit to byte
			iSndRetval = BitArrayToByteArray(buf, len, bufSend, len / 8 + 1);
			iSndRetval = SendtoLower(bufSend, iSndRetval, target);
			iSndRetval = iSndRetval * 8;//�����λ����ͳ��
		}
	}
	//ͳ��
	if (iSndRetval <= 0) {
		iSndErrorCount++;
	}
	else {
		iRcvToUpper += iSndRetval;
		iRcvToUpperCount++;
	}
	free(bufSend);
}

//�㲥���͸����������ÿһ��ʵ��
void broadcast(U8* buf, int len) {
	int i = 0;
	printf("lowerNum: %d\n", lowerNumber);
	for (int times = 0; times < lowerNumber; times++) {
		if (times != mapMac[getMac(buf)]) {
			send2low(buf, len, times);
		}
	}
}

//��ӡ��ַ��
void printChart() {
	printf("**************************mac port chart**************************\n");
	for (iter = mapMac.begin(); iter != mapMac.end(); iter++)
		cout << "	mac(enity id): " << iter->first << "------------------port: " << iter->second << endl;
	printf("******************************************************************\n");
}

//����
void unicast(U8* buf, int len) {
	U8* bufSend = NULL;
	int iSndRetval = 0;
	int switchSource = 0;
	int switchTarget = 0;
	switchSource = getMac(buf);
	switchTarget = buf[12] * 4 + buf[13] * 2 + buf[14];
	printf("target: %d\n", switchTarget);
	//���mac��֪
	if (mapMac.count(switchTarget)) {
		printChart();
		printf("KNOWN MAC\n");
		send2low(buf, len, mapMac[switchTarget]);
	}
	else {
		printf("UNKNOWN MAC\n");
		//�㲥
		broadcast(buf, len);
		if (!(mapMac.count(switchTarget))) {
			mapMac[switchTarget] = 0;
		}
	}
}
//��ӡ������
void printBuf(U8* buf) {
	printf("\n|||||||||||||||printbuf||||||||||||||||\n");
	for (int i = 0; i < getLen(buf); i++)
		printf("%d ", buf[i]);
	printf("\n|||||||||||||||printbuf||||||||||||||||\n");
}
//��ӡ����
void printBackUp(int sequence) {
	printf("\n**************************Back-----Up*****************************\n");
	for (int i = 0; i < getLen(saveBuf[sequence]); i++)
		printf("%d ", saveBuf[sequence][i]);
	printf("\n******************************************************************\n");
}
//***************��Ҫ��������******************************
//���ƣ�TimeOut
//���ܣ�������������ʱ����ζ��sBasicTimer�����õĳ�ʱʱ�䵽�ˣ�
//      �������ݿ���ȫ���滻Ϊ������Լ����뷨
//      ������ʵ���˼���ͬʱ���й��ܣ����ο�
//      1)����iWorkMode����ģʽ���ж��Ƿ񽫼�����������ݷ��ͣ�
//        ��Ϊscanf�����������¼�ʱ���ڵȴ����̵�ʱ����ȫʧЧ������ʹ��_kbhit()������������ϵ��ڼ�ʱ�Ŀ������жϼ���״̬�������Get��û��
//      2)����ˢ�´�ӡ����ͳ��ֵ��ͨ����ӡ���Ʒ��Ŀ��ƣ�����ʼ�ձ�����ͬһ�д�ӡ��Get��
//���룺ʱ�䵽�˾ʹ�����ֻ��ͨ��ȫ�ֱ�����������
//���������Ǹ�����Ŭ���ɻ����ʵ����
int seq = 1;
int resendSeq = 0;
int isResend[200] = { -1 };
int recvSeq = 1;
void TimeOut() {
	int switchSource = 0;
	int switchTarget = 0;
	int sequence = 0;
	int i = 0;
	//����������
	if (isTimerStart == true) {
		//tickTack++;
		if (tickTack % 1 == 0) {
			//�㲥or����
			if ((switchBuf.count(seq))) {
				if (switchBuf[seq][frameHeadLen]) {
					//�㲥
					printf("\nBROAD\n");
					if (switchBuf.count(seq)) {
						broadcast(switchBuf[seq], getLen(switchBuf[seq]));
					}
				}
				else {
					//����
					//��ȡĿ��mac
					printf("\nUNICAST\n");
					unicast(switchBuf[seq], getLen(switchBuf[seq]));
				}
				preSequence = sequence;
				seq++;
			}
		}
	}
	//�ش�
	if (isTimerStart1 == true) {
		if (tickTack1 % 1 == 0) {
			if (!(resendQueue.empty())) {
				printf("\n//////////////resend through switch/////////////\n");
				sequence = resendQueue.front();
				resendQueue.pop();
				if (switchBuf[sequence][frameHeadLen]) {
					//�㲥
					printf("\nBROAD\n");
					if (switchBuf.count(sequence)) {
						switchSource = switchBuf[sequence][9] * 4 + switchBuf[sequence][10] * 2 + switchBuf[sequence][11];
						switchTarget = switchBuf[sequence][12] * 4 + switchBuf[sequence][13] * 2 + switchBuf[sequence][14];
						broadcast(switchBuf[sequence], getLen(switchBuf[sequence]));
					}
				}
				else {
					//����
					//��ȡĿ��mac
					printf("\nUNICAST\n");
					unicast(switchBuf[sequence], getLen(switchBuf[sequence]));
				}
				//isTimerStart = false;
			}
		}
	}
	//���ó�ʱ�ش�
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
		//�����ж���������˵�ģʽ
		menu();
	}

	print_statistics();
}
//------------�����ķָ��ߣ����������ݵ��շ�,--------------------------------------------

//��1��0��֡ͷ���ȣ������ݣ� ����λ���ݳ��ȣ�����������
int addZero(int head, U8* sendBitArray, int len, U8* sendBitArray1) {
	int iSndRetval = 0; //�������
	int oneCount = 0; //��1����
	int i;
	int addZeroLength = 0; //��ʼ��������
	sendBitArray1[head] = sendBitArray[head];
	//�����ݵĵڶ�λ��ʼ
	//�Ͻ��ȷ���������ַ����Դ�'\0'��-8ȥ����ֹ����X��
	for (i = head + 1, addZeroLength = head + 1; i < len + head; i++, addZeroLength++) {
		//printf("\naddZeroLength=%d\n", oneCount);
		//printf("buf[i]: %d, buf[i-1]: %d\n", sendBitArray[i], sendBitArray[i - 1]);
		sendBitArray1[addZeroLength] = sendBitArray[i];
		//����ж��ǲ��ǳ���������5��1
		if (sendBitArray[i] == 1 && sendBitArray[i - 1] == 1) {
			oneCount++;
		}
		else {
			oneCount = 0;
		}
		if (oneCount == 4) {
			//����bitArray[i]��5��������1
			addZeroLength++;
			sendBitArray1[addZeroLength] = 0;  //����1����
			oneCount = 0;
		}
	}
	return addZeroLength;
}

//ͬ��֡�����ݣ��ܳ�
void frameAlignment(U8* sendBitArray, int dataLen) {
	//��
	int i;
	sendBitArray[0] = 0;
	sendBitArray[frameHeadLen - 1] = 0;
	for (i = 1; i < frameHeadLen - 1; i++) {
		sendBitArray[i] = 1;
	}
	//β
	sendBitArray[dataLen - 8] = 0;
	sendBitArray[dataLen - 1] = 0;
	for (i = dataLen - 7; i < dataLen - 1; i++) {
		sendBitArray[i] = 1;
	}
}

//��ʼ��mac��
void macInit(U8* sendBitArray, U8* mac) {
	U8* mac8 = (U8*)malloc(8);
	ByteArrayToBitArray(mac8, 8, mac, 1);
	for (int i = 8; i < 16; i++)
		sendBitArray[i] = mac8[i - 8];
	free(mac8);
}

//���м���
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

//���crc
void crc(U8* sendBitArray, int dataLen){
	//CRC
	//generate
	int zero_count = 0; //��0����
	int i;
	U8* sendBitArray0 = NULL;
	U8 generateCode[9] = { 1, 0, 0, 0, 0, 0, 1, 1, 1 };
	zero_count = 0;
	sendBitArray0 = (U8*)malloc(dataLen + 8);
	//copy
	//��0
	for (i = dataLen - (frameHeadLen + macLen + frameNumLen); i < dataLen - (frameHeadLen + macLen + frameNumLen) + 8; i++) {
		sendBitArray0[i] = 0;
	}
	//ԭ����
	for (i = frameHeadLen + macLen + frameNumLen; i < dataLen; i++) {
		sendBitArray0[i - (frameHeadLen + macLen + frameNumLen)] = sendBitArray[i];
	}
	//ģ����
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
	//���ս����[0]~[7]
	for (i = dataLen; i < dataLen + 8; i++) {
		sendBitArray[i] = sendBitArray0[i - dataLen];
	}
	free(sendBitArray0);
}

//��ȡ����
int getSequence(U8* buft) {
	int sequence = 0, i = 0;
	for (i = 16; i < 24; i++) {
		//printf("seq buft[i]: %d\n", buft[i]);
		sequence += int(pow(2, (23 - i))) * buft[i];
	}
	return sequence;
}

//����
void backUp(U8* sendBitArray, int dataLen) {
	int i;
	int sequence = getSequence(sendBitArray);
	lengthSave = dataLen + crcLen + frameBackLen;
	saveBuf[sequence] = (U8*)malloc(lengthSave);
	for (i = 0 ; i < lengthSave; i++)
		saveBuf[sequence][i] = sendBitArray[i];
	//ȫ�ֱ�������
}
//***************��Ҫ��������******************************
//���ƣ�RecvfromUpper
//���ܣ�������������ʱ����ζ���յ�һ�ݸ߲��·�������
//      ��������ȫ�������滻��������Լ���
//      ���̹��ܽ���
//         1)ͨ���Ͳ�����ݸ�ʽ����lowerMode���ж�Ҫ��Ҫ������ת����bit�����鷢�ͣ�����ֻ�����Ͳ�ӿ�0��
//           ��Ϊû���κοɹ��ο��Ĳ��ԣ���������Ӧ�ø���Ŀ�ĵ�ַ�ڶ���ӿ���ѡ��ת���ġ�
//         2)�ж�iWorkMode�������ǲ�����Ҫ�����͵��������ݶ���ӡ������ʱ���ԣ���ʽ����ʱ�����齫����ȫ����ӡ��
//���룺U8 * buf,�߲㴫���������ݣ� int len�����ݳ��ȣ���λ�ֽ�
//�����
void RecvfromUpper(U8* buf, int len){	
	int iSndRetval; //�������
	U8* bufSend = NULL;
	int i = 0, j = 0;
	//��ȡ���ĵ�ַ��Ϣ��������buf1
	U8* saveMac = (U8*)malloc(1);
	saveMac[0] = buf[len - 1];
	len--;
	U8* buf1 = (U8*)malloc(len);
	for (i = 0; i < len; i++) {
		buf1[i] = buf[i];
		//printf("%d ", buf[i]);
	}

	//�Ǹ߲����ݣ�ֻ�ӽӿ�0����ȥ,�߲�ӿ�Ĭ�϶����ֽ������ݸ�ʽ
	if (lowerMode[0] == 0) {
		//�ӿ�0��ģʽΪbit���飬��ת����bit���飬�ŵ�bufSend��
		bufSend = (U8*)malloc(len * 8 + frameHeadLen + macLen + frameNumLen + crcLen + frameBackLen);
		//+xx: ��len����8�����������ݵ��׵�ַ
		iSndRetval = ByteArrayToBitArray(bufSend + frameHeadLen + macLen + frameNumLen, len * 8, buf1, len);
		//mac
		macInit(bufSend, saveMac);
		//�������
		sequenceCount(bufSend);
		//crc
		crc(bufSend, iSndRetval + 24);
		//*16: �㹻�ռ���0��+xx�� ��װ�ռ�
		U8* sendBitArray1 = (U8*)malloc(len * 16 + 40);
		//��1��0
		int addLen = addZero(frameHeadLen, bufSend, len * 8 + 24, sendBitArray1);
		int addLen1 = addLen + 8;
		//֡ͬ��
		frameAlignment(sendBitArray1, addLen1);
		//printBuf(sendBitArray1);
		//����
		iSndRetval = SendtoLower(sendBitArray1, addLen1, 0); //��������Ϊ���ݻ��壬���ȣ��ӿں�
		//printBuf(sendBitArray1);
		//���ͺ�ʼ��ʱ����ʱ���ش�
		isTimerStart2 = true;
		tickTack2 = 0;
		backUp(sendBitArray1, addLen1);
		//printf("\ndataLen + 16: %d\n", dataLen + 16);
		//print_data_bit(sendBitArray1, addLen1, 0);
	}
	else {
		//�²����ֽ�����ӿڣ���ֱ�ӷ���
		iSndRetval = SendtoLower(buf1, len, 0);
		iSndRetval = iSndRetval * 8;//�����λ
	}
	//ͳ��
	if (iSndRetval <= 0) {
		iSndErrorCount++;
	}
	else {
		iSndTotal += iSndRetval;
		iSndTotalCount++;
	}
	//printf("\n�յ��ϲ����� %d λ�����͵��ӿ�0\n", retval * 8);
	//��ӡ
	switch (iWorkMode % 10) {
	case 1:
		cout << endl << "�߲�Ҫ����ӿ� " << 0 << " �������ݣ�" << endl;
		print_data_bit(buf1, len, 1);
		break;
	case 2:
		cout << endl << "�߲�Ҫ����ӿ� " << 0 << " �������ݣ�" << endl;
		print_data_byte(buf1, len, 1);
		break;
	case 0:
		break;
	}
	free(buf1);
}

//���������
void send2phy(U8* buf, int len, int ifNo) {
	U8* buf2 = (U8*)malloc(2 * len);
	int leng = addZero(8, buf, len - 16, buf2);
	frameAlignment(buf2, leng + 8);
	SendtoLower(buf2, leng + 8, ifNo);
	free(buf2);
}

//��ȡ��ԭ����ԭ���ݣ���ȡ������ݣ��������ݳ���
int extract(int len, U8* buf, U8* buft) {
	int arrLength = 0; //���մ����ĳ���
	int flag1 = 0;
	int flag2 = 0;//λ��1 2
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
	//��������,cut
	for (i = 0; i < arrLength; i++) {
		buft[i] = buf[i + flag1];
	}
	return arrLength;
}

//����crc
void checkCrc(U8* buft, U8* sendBitArray0, int arrLength) {
	int zero_count = 0;
	int i;
	printf("\nChecking CRC...");
	//check crc
	U8 generateCode[9] = { 1, 0, 0, 0, 0, 0, 1, 1, 1 };
	zero_count = 0;
	//copy
	//����+crc
	for (i = frameHeadLen + macLen + frameBackLen; i < arrLength - frameBackLen; i++) {
		sendBitArray0[i - (frameHeadLen + macLen + frameBackLen)] = buft[i];
	}
	//ģ����
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


//����mac��
void updateMac(U8* buf, int ifNo) {
	//����mac
	int id = getMac(buf);
	mapMac[id] = ifNo;
}

//��ȷ��֡
void sendAck(U8* ack, bool flag, U8* buf, int ifNo) {
	int i;	
	int id;
	for (i = 0; i < 32; i++)
		ack[i] = 0;
	//�ж���ȷ���
	if (!(flag))
		ack[15] = 0;
	else
		ack[15] = 1;
	//��װ
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

//��ԭ��1
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

//�ش�
void resend(int sequence, int ifNo) {
	printf("RESEND------------len: %d----------sequence: %d\n", getLen(saveBuf[sequence]), sequence);
	//printBackUp(sequence);
	SendtoLower(saveBuf[sequence], getLen(saveBuf[sequence]), 0);
	tickTack2 = 1;
}

//***************��Ҫ��������******************************
//���ƣ�RecvfromLower
//���ܣ�������������ʱ����ζ�ŵõ�һ�ݴӵͲ�ʵ��ݽ�����������
//      ��������ȫ�������滻���������Ҫ������
//      ���̹��ܽ��ܣ�
//          1)����ʵ����һ���򵥴ֱ���������Ĳ��ԣ����дӽӿ�0�����������ݶ�ֱ��ת�����ӿ�1�����ӿ�1�������Ͻ����߲㣬������ô����
//          2)ת�����Ͻ�ǰ���ж��ս����ĸ�ʽ��Ҫ���ͳ�ȥ�ĸ�ʽ�Ƿ���ͬ��������bite��������ֽ�������֮��ʵ��ת��
//            ע����Щ�жϲ������������ݱ�����������������������ļ������������ļ��Ĳ���д���ˣ��ж�Ҳ�ͻ�ʧ��
//          3)����iWorkMode���ж��Ƿ���Ҫ���������ݴ�ӡ
//���룺U8 * buf,�Ͳ�ݽ����������ݣ� int len�����ݳ��ȣ���λ�ֽڣ�int ifNo ���Ͳ�ʵ����룬�����������ĸ��Ͳ�
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
		//1��ԭ
		return2one(8, 8, buft0, buft, arrLength);
		sequence = getSequence(buft0);
		printf("\n................sequen: %d..............\n", sequence);
		printf("arrlen: %d\n", arrLength);
		//print_data_bit(buft, arrLength, 0);
		//if (sequence == 35) {
		//	for (i = 0; i < 800; i++)
		//		printf("%d ", buft0[i]);
		//}
		//����ǽ�����
		if (lowerNumber > 1) {
			//��Ϊack��ѧϰ��ֱ�ӷ���
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
				//����Դ��ַmac��ѧϰ
				if (!(mapMac.count(getMac(buft0)))) {
					updateMac(buft0, ifNo);
					printChart();
				}
				//����ͬ, ����ѧϰ��ʡ�ԣ�
				//�洢��ת��
				isResend[sequence] += 1;
				switchBuf[sequence] = (U8*)malloc(getLen(buft0));
				int newLen = addZero(8, buft0, arrLength - 16, switchBuf[sequence]);
				frameAlignment(switchBuf[sequence], newLen + 8);
				if (isResend[sequence] >= 2) {
					//����Ҫ�ش�������ѹ�����
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
		//������ǽ�������֡���ȴ���32��˵�������ݱ��ģ����и��ִ���
		else if (arrLength > 32) {
			//����ǰ�ش���������ȷ���գ�����
			if (recvBuf.count(sequence)) {
				printf("ALREADY EXIST, ABANDON SEQUENCE: %d\n", sequence);
			}
			else {
				//��ȡcrc
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
				//��Ϊ��ȷ֡
				if (flag) {
					//���մ����ݴ�
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
								//����ӿ�0�Ǳ��������ʽ���߲�Ĭ�����ֽ����飬��ת�����ֽ����飬�����ϵݽ�
								bufSend2 = (U8*)malloc(getLen(recvBuf[seqCount]) / 8);
								iSndRetval2 = BitArrayToByteArray(recvBuf[seqCount], getLen(recvBuf[seqCount]), bufSend2, getLen(recvBuf[seqCount]) / 8);
								iSndRetval2 = SendtoUpper(bufSend2, iSndRetval2);
							}
							else {
								//�Ͳ����ֽ�����ӿڣ���ֱ�ӵݽ�
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
		//�������Ϊ32��˵����ȷ��֡
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
					//����ѽ���
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
	
	//��ӡ
	switch (iWorkMode % 10) {
	case 1:
		cout << endl << "���սӿ� " << ifNo << " ���ݣ�" << endl;
		print_data_bit(buf, len, lowerMode[ifNo]);
		break;
	case 2:
		cout << endl << "���սӿ� " << ifNo << " ���ݣ�" << endl;
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
		cout << "��ת�� "<< iRcvForward<< " λ��"<< iRcvForwardCount<<" �Σ�"<<"�ݽ� "<< iRcvToUpper<<" λ��"<< iRcvToUpperCount<<" ��,"<<"���� "<< iSndTotal <<" λ��"<< iSndTotalCount<<" �Σ�"<< "���Ͳ��ɹ� "<< iSndErrorCount<<" ��,""�յ�������Դ "<< iRcvUnknownCount<<" �Ρ�";
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
	//����|��ӡ��[���Ϳ��ƣ�0���ȴ��������룻1���Զ���][��ӡ���ƣ�0�������ڴ�ӡͳ����Ϣ��1����bit����ӡ���ݣ�2���ֽ�����ӡ����]
	cout << endl << endl << "�豸��:" << strDevID << ",    ���:" << strLayer << ",    ʵ���:" << strEntity;
	cout << endl << "1-�����Զ�����(��Ч);" << endl << "2-ֹͣ�Զ����ͣ���Ч��; " << endl << "3-�Ӽ������뷢��; ";
	cout << endl << "4-����ӡͳ����Ϣ; " << endl << "5-����������ӡ��������;" << endl << "6-���ֽ�����ӡ��������;";
	cout << endl << "0-ȡ��" << endl << "����������ѡ�����";
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
		cout << "�����ַ���(,������100�ַ�)��";
		cin >> kbBuf;
		cout << "����Ͳ�ӿںţ�";
		cin >> port;

		len = (int)strlen(kbBuf) + 1; //�ַ�������и�������
		if (port >= lowerNumber) {
			cout << "û������ӿ�" << endl;
			return;
		}
		if (lowerMode[port] == 0) {
			//�²�ӿ��Ǳ���������,��ҪһƬ�µĻ�����ת����ʽ
			bufSend = (U8*)malloc(len * 8);

			iSndRetval = ByteArrayToBitArray(bufSend, len * 8, kbBuf, len);
			iSndRetval = SendtoLower(bufSend, iSndRetval, port);
		}
		else {
			//�²�ӿ����ֽ����飬ֱ�ӷ���
			iSndRetval = SendtoLower(kbBuf, len, port);
			iSndRetval = iSndRetval * 8; //�����λ
		}
		//����ͳ��
		if (iSndRetval > 0) {
			iSndTotalCount++;
			iSndTotal += iSndRetval;
		}
		else {
			iSndErrorCount++;
		}
		//��Ҫ��Ҫ��ӡ����
		cout << endl << "��ӿ� " << port << " �������ݣ�" << endl;
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
//��·��������װ��֡�滮��Э��ѡ��mac�滮��������δ������ʹ�����м�������0������ȥ0