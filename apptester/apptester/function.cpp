//apptester�Ĺ����ļ�
#include <iostream>
#include <conio.h>
#include <sstream>
#include "winsock.h"
#include "stdio.h"
#include "CfgFileParms.h"
#include "function.h"
using namespace std;

U8* autoSendBuf;        //������֯�������ݵĻ��棬��СΪMAX_BUFFER_SIZE,���������������������ƣ��γ��ʺϵĽṹ��������û��ʹ�ã�ֻ������һ��
int printCount = 0; //��ӡ����
int spin = 1;  //��ӡ��̬��Ϣ����
int source = 0;
int target = 0;
bool judgement = false;
//------�����ķָ��ߣ�һЩͳ���õ�ȫ�ֱ���------------
int iSndTotal = 0;  //������������
int iSndTotalCount = 0; //���������ܴ���
int iSndErrorCount = 0;  //���ʹ������
int iRcvTotal = 0;     //������������
int iRcvTotalCount = 0; //ת�������ܴ���
int iRcvUnknownCount = 0;  //�յ�������Դ�����ܴ���

void print_statistics();
void menu();
//***************��Ҫ��������******************************
//���ƣ�InitFunction
//���ܣ���ʼ�������棬��main�����ڶ��������ļ�����ʽ������������ǰ����
//���룺
//�����
void InitFunction(CCfgFileParms& cfgParms)
{
	int i;
	int retval;
	
	retval = cfgParms.getValueInt(autoSendTime, (char*)"autoSendTime");
	if (retval == -1 || autoSendTime == 0) {
		autoSendTime = DEFAULT_AUTO_SEND_TIME;
	}
	retval = cfgParms.getValueInt(autoSendSize, (char*)"autoSendSize");
	if (retval == -1 || autoSendSize == 0) {
		autoSendSize = DEFAULT_AUTO_SEND_SIZE;
	}

	autoSendBuf = (char*)malloc(MAX_BUFFER_SIZE);
	if (autoSendBuf == NULL) {
		cout << "�ڴ治��" << endl;
		//����������Ҳ̫���˳���
		exit(0);
	}
	for (i = 0; i < MAX_BUFFER_SIZE; i++) {
		autoSendBuf[i] = 'a'; //��ʼ������ȫΪ�ַ�'a',ֻ��Ϊ�˲���
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
	if (autoSendBuf != NULL)
		free(autoSendBuf);
	return;
}

//***************��Ҫ��������******************************
//���ƣ�TimeOut
//���ܣ�������������ʱ����ζ��sBasicTimer�����õĳ�ʱʱ�䵽�ˣ�
//      �������ݿ���ȫ���滻Ϊ������Լ����뷨
//      ������ʵ���˼���ͬʱ���й��ܣ����ο�
//      1)����iWorkMode����ģʽ���ж��Ƿ񽫼�����������ݷ��ͣ������Զ����͡������������ʵ��Ӧ�ò��
//        ��Ϊscanf�����������¼�ʱ���ڵȴ����̵�ʱ����ȫʧЧ������ʹ��_kbhit()������������ϵ��ڼ�ʱ�Ŀ������жϼ���״̬�������Get��û��
//      2)����ˢ�´�ӡ����ͳ��ֵ��ͨ����ӡ���Ʒ��Ŀ��ƣ�����ʼ�ձ�����ͬһ�д�ӡ��Get��
//      3)�����iWorkMode����Ϊ�Զ����ͣ���ÿ����autoSendTime * DEFAULT_TIMER_INTERVAL ms����ӿ�0����һ��
//���룺ʱ�䵽�˾ʹ�����ֻ��ͨ��ȫ�ֱ�����������
//���������Ǹ�����Ŭ���ɻ����ʵ����
void TimeOut()
{
	int iSndRetval;
	int len;
	U8* bufSend;
	int i, j, k;
	U8* bufSend1;

	printCount++;
	if (_kbhit()) {
		//�����ж���������˵�ģʽ
		menu();
	}
	switch (iWorkMode / 10) {
	case 0:
		break;
	case 1:
		//��ʱ����, ÿ���autoSendTime * DEFAULT_TIMER_INTERVAL ms ����һ��
		if (printCount % autoSendTime == 0) {
			for (i = 0; i < min(autoSendSize, 8); i++) {
				//��ͷ�����ֽ���26����ĸ�м����������ڹ۲�
				autoSendBuf[i] = 'a' + printCount % 26;
			}
			len = autoSendSize; //ÿ�η�������
			if (lowerMode[0] == 0) {
				//�Զ�����ģʽ�£�ֻ��ӿ�0����
				bufSend = (U8*)malloc(len * 8);
				//�²�ӿ��Ǳ���������
				iSndRetval = ByteArrayToBitArray(bufSend, len * 8, autoSendBuf, len);
				iSndRetval = SendtoLower(bufSend, iSndRetval, 0);
				free(bufSend);
			}
			else {
				//�²�ӿ����ֽ����飬ֱ�ӷ���
				for (i = 0; i < min(autoSendSize, 8); i++) {
					//��ͷ�����ֽ���26����ĸ�м����������ڹ۲�
					autoSendBuf[i] = 'a' + printCount % 26;
				}
				//��װ��ַ
				len = autoSendSize + 1; //�ַ������
				bufSend1 = (U8*)malloc(len * 8); 
				iSndRetval = ByteArrayToBitArray(bufSend1, len * 8, autoSendBuf, len); 
				for (i = iSndRetval - 8; i <= iSndRetval - 1; i++)
					bufSend1[i] = 0;
				if (judgement)
					bufSend1[iSndRetval - 8] = 1;
				j = iSndRetval - 2;
				int target1 = target;
				int source1 = source;
				j = iSndRetval - 2;
				while (target1) {
					bufSend1[j] = target1 % 2;
					target1 /= 2;
					j--;
				}
				k = iSndRetval - 5;
				while (source1) {
					bufSend1[k] = source1 % 2;
					source1 /= 2;
					k--;
				}
				len = BitArrayToByteArray(bufSend1, iSndRetval, autoSendBuf, iSndRetval / 8);
				free(bufSend1);
				iSndRetval = SendtoLower(autoSendBuf, len, 0);
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
			cout << endl << "���սӿ� " << 0 << " ���ݣ�" << endl;
			print_data_byte(autoSendBuf, len, 1);
			switch (iWorkMode % 10) {
			case 1:
				print_data_bit(autoSendBuf, len, 1);
				break;
			case 2:
				print_data_byte(autoSendBuf, len, 1);
				break;
			case 0:
				break;
			}
		}

		break;
	}
	//���ڴ�ӡͳ������
	print_statistics();

}
//------------�����ķָ��ߣ����������ݵ��շ�,--------------------------------------------

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
void RecvfromUpper(U8* buf, int len)
{
	//Ӧ�ò㲻���յ����߲㡱�����ݣ������Լ�����
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
//�����
void RecvfromLower(U8* buf, int len, int ifNo)
{
	int retval;
	U8* bufRecv = NULL;
	if (lowerMode[ifNo] == 0) {
		//�Ͳ���bit�������ʽ����Ҫת�����ŷ����ӡ
		bufRecv = (U8*)malloc(len / 8 + 1);
		if (bufRecv == NULL) {
			return;
		}
		//����ӿ�0�Ǳ��������ʽ����ת�����ֽ����飬�����ϵݽ�
		retval = BitArrayToByteArray(buf, len, bufRecv, len / 8 + 1);
		retval = len;
	}
	else {
		retval = len * 8;//�����λ,����ͳ��
	}
	iRcvTotal += retval;
	iRcvTotalCount++;

	//��ӡ
	cout << endl << "���սӿ� " << ifNo << " ���ݣ�" << endl;
	print_data_byte(buf, len, lowerMode[ifNo]);
	switch (iWorkMode % 10) {
	case 1:
		cout <<endl<< "���սӿ� " <<ifNo <<" ���ݣ�"<<endl;
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
//��ӡͳ����Ϣ
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
		cout << "������ " << iSndTotal << " λ," << iSndTotalCount << " ��," << "���� " << iSndErrorCount << " �δ���;";
		cout << " ������ " << iRcvTotal << " λ," << iRcvTotalCount << " ��" ;
		spin++;
	}
}

int string2int(string a) {
	stringstream stream;
	stream << a;
	int b;
	stream >> b;
	return b;
}

void menu()
{
	int selection;
	unsigned short port;
	int iSndRetval;
	char kbBuf[100]; 
	int len, j, k;
	CCfgFileParms* cfg;
	U8* bufSend = NULL;
	target = -1;
	//����|��ӡ��[���Ϳ��ƣ�0���ȴ��������룻1���Զ���][��ӡ���ƣ�0�������ڴ�ӡͳ����Ϣ��1����bit����ӡ���ݣ�2���ֽ�����ӡ����]
	cout << endl << endl << "�豸��:" << strDevID << ",    ���:" << strLayer << ",    ʵ���:" << strEntity;
	cout << endl << "1-�����Զ�����;" << endl << "2-ֹͣ�Զ�����; " << endl << "3-�Ӽ������뷢��; ";
	cout << endl << "4-����ӡͳ����Ϣ; " << endl << "5-����������ӡ��������;" << endl << "6-���ֽ�����ӡ��������;";
	cout << endl << "0-ȡ��" << endl << "����������ѡ�����";
	cin >> selection;
	switch (selection) {
	case 0:
		break;
	case 1:
		cout << "Your Enity Number: " << strDevID << endl;
		cout << "The enity you can send to: 1��2��3��4��5��6" << endl;
		//cout << "Boraodcast(1) or Unicast(0): ";
		judgement = false;
		if (judgement == false) {
			cout << "Your Target: ";
			cin >> target;
		}
		else {
			target = 1;
		}
		target--;
		source = string2int(strDevID) - 1;
		iWorkMode = 10 + iWorkMode % 10; 
		break;
	case 2:
		iWorkMode = iWorkMode % 10;
		break;
	case 3:	
		cout << "�����ַ���(������100�ַ�)��";
		cin >> kbBuf;
		cout << "�Ͳ�ӿںţ�0\n";
		port = 0;
		cout << "Your Enity Number: " << strDevID << endl;
		cout << "The enity you can send to: 1��2��3��4��5��6" << endl;
		cout << "Boraodcast(1) or Unicast(0): ";
		cin >> judgement;
		//��װ��ַ
		if (judgement == false) {
			cout << "Your Target: ";
			cin >> target;
		}
		else {
			target = 1;
		}
		source = string2int(strDevID) - 1;
		len = (int)strlen(kbBuf) + 2; //�ַ�������и�������
		bufSend = (U8*)malloc(len * 8);
		iSndRetval = ByteArrayToBitArray(bufSend, len * 8, kbBuf, len);
		for (int i = iSndRetval - 8; i <= iSndRetval - 1; i++)
			bufSend[i] = 0;
		if (judgement)
			bufSend[iSndRetval - 8] = 1;	

		j = iSndRetval - 2;
		target--;
		while (target) {
			bufSend[j] = target % 2;
			target /= 2;
			j--;
		}
		k = iSndRetval - 5;
		while (source) {
			bufSend[k] = source % 2;
			source /= 2;
			k--;
		}
		len = BitArrayToByteArray(bufSend, iSndRetval, kbBuf, iSndRetval / 8);
		free(bufSend);
		if (port >= lowerNumber) {
			cout << "û������ӿ�" << endl;
			return;
		}
		
		if (lowerMode[port] == 0) {
			//�²�ӿ��Ǳ���������,��ҪһƬ�µĻ�����ת����ʽ
			bufSend = (U8*)malloc(len * 8);

			iSndRetval = ByteArrayToBitArray(bufSend, len * 8, kbBuf, len);
			iSndRetval = SendtoLower(bufSend, iSndRetval, port);
			free(bufSend);
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
		print_data_bit(kbBuf, len, 1);
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

