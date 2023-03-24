//Nettester �Ĺ����ļ�
#include <iostream>
#include <conio.h>
#include "winsock.h"
#include "stdio.h"
#include "CfgFileParms.h"
#include "function.h"
#include <time.h>
#include <map>
#include <sstream>

using namespace std;

//����Ϊ��Ҫ�ı���
U8* sendbuf;        //������֯�������ݵĻ��棬��СΪMAX_BUFFER_SIZE,���������������������ƣ��γ��ʺϵĽṹ��������û��ʹ�ã�ֻ������һ��
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

int port = 2;
//·�ɱ�
int router[2][4][8] = { {{1, 2, 3, 4, 5, 6, 7, 8}, {-1, 2, 7, 7, 2, 2, 7, 2}, {-1, 1, 0, 0, 1, 1, 0, 1}, {0, 1, 2, 2, 3, 3, 1, 2}},
	{{1, 2, 3, 4, 5, 6, 7, 8}, {1, -1, 1, 1, 8, 8, 1, 8}, {1, -1, 1, 1, 0, 0, 1, 0}, {1, 0, 3, 3, 2, 2, 2, 1}} };

//��ӡ·�ɱ�
void printRoutingTable() {
	int i, j, k;
	for (i = 0; i < 2; i++) {
		printf("router%d: \n", i + 1);
		for (j = 0; j < 4; j++) {
			if (j == 0) {
				printf("Destination: \n");
			}
			if (j == 1) {
				printf("Gateway: \n");
			}
			if (j == 2) {
				printf("Port: \n");
			}
			if (j == 3) {
				printf("Metric: \n");
			}
			for (k = 0; k < 8; k++) {
				printf("%d ", router[i][j][k]);
				if (router[i][j][k] >= 0)
					printf(" ");
			}
			printf("\n");
		}
		printf("\n");
	}
}
//��ȡ����
int getLen(U8* buf) {
	int i = 0;
	while (buf[i] == 0 || buf[i] == 1)
		i++;
	return i;
}
//��ӡ������
void printBuf(U8* buf) {
	printf("\n------------------------------------------------\n");
	for (int i = 0; i < getLen(buf); i++)
		printf("%d ", buf[i]);
	printf("\n------------------------------------------------\n");
}
//��ӡ�ֽ���
void printByteBuf(U8* buf, int len) {
	printf("\n------------------------------------------------\n");
	for (int i = 0; i < len; i++)
		printf("%c ", buf[i]);
	printf("\n------------------------------------------------\n");
}
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
//��ȡԴ
int getSource(U8* buf) {
	return buf[9] * 4 + buf[10] * 2 + buf[11];
}
//��ȡĿ��
int getTarget(U8* buf) {
	return buf[12] * 4 + buf[13] * 2 + buf[14];
}
//�ַ���ת����
int string2int(string a) {
	stringstream stream;
	stream << a;
	int b;
	stream >> b;
	return b;
}
//���ҽӿ�
int findPort(U8* buf) {
	int source = getSource(buf);
	int target = getTarget(buf);
	int route = string2int(strDevID) - 1;
	return router[route][port][target];
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
void TimeOut()
{

	printCount++;
	if (_kbhit()) {
		//�����ж���������˵�ģʽ
		menu();
	}

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
	int iSndRetval;
	U8* bufSend = NULL;
	//�Ǹ߲����ݣ�ֻ�ӽӿ�0����ȥ,�߲�ӿ�Ĭ�϶����ֽ������ݸ�ʽ
	if (lowerMode[0] == 0) {
		//�ӿ�0��ģʽΪbit���飬��ת����bit���飬�ŵ�bufSend��
		bufSend = (char*)malloc(len * 8);
		if (bufSend == NULL) {
			return;
		}
		iSndRetval = ByteArrayToBitArray(bufSend, len * 8, buf, len);
		//����
		iSndRetval = SendtoLower(bufSend, iSndRetval, 0); //��������Ϊ���ݻ��壬���ȣ��ӿں�
	}
	else {
		//�����·����
		if (lowerNumber > 1) {
			U8* bufsend1 = (U8*)malloc(len * 8);
			int leng = ByteArrayToBitArray(bufsend1, len * 8, buf, len);
			int route = string2int(strDevID) - 1;
			int target = bufsend1[leng - 4] * 4 + bufsend1[leng - 3] * 2 + bufsend1[leng - 2];
			//���
			int getPort = router[route][port][target];
			printf("\nport: %d\n", port);
			printRoutingTable();
			//����port������
			if (port >= 0) {
				printf("Sending to others\n");
				iSndRetval = SendtoLower(buf, len, getPort);
				iSndRetval = iSndRetval * 8;//�����λ
			}
		}
		else {
			//�²����ֽ�����ӿڣ���ֱ�ӷ���
			iSndRetval = SendtoLower(buf, len, 0);
			iSndRetval = iSndRetval * 8;//�����λ
		}

	}
	//����������ͣ��Э����ش�Э�飬���������Ҫ����������Ӧ����������ռ䣬��buf��bufSend�����ݱ����������Ա��ش�
	if (bufSend != NULL) {
		//����bufSend���ݣ�CODES NEED HERE
		//������û������ش�Э�飬����Ҫ�������ݣ����Խ��ռ��ͷ�
		free(bufSend);
	}

	//ͳ��
	if (iSndRetval <= 0) {
		iSndErrorCount++;
	}
	else {
		iSndTotal += iSndRetval;
		iSndTotalCount++;
	}
	//��ӡ
	switch (iWorkMode % 10) {
	case 1:
		cout << endl << "�߲�Ҫ����ӿ� " << 0 << " �������ݣ�" << endl;
		print_data_bit(buf, len, 1);
		break;
	case 2:
		cout << endl << "�߲�Ҫ����ӿ� " << 0 << " �������ݣ�" << endl;
		print_data_byte(buf, len, 1);
		break;
	case 0:
		break;
	}

}
//�����²�
void send2low(U8* buf, int len, int port) {
	//���Ȼ�ԭbyte��ʽ
	U8* bufSend = (U8*)malloc(len - 24);
	for (int i = 0; i < len - 32 - 8; i++) {
		bufSend[i] = buf[i + 24];
	}
	for (int i = len - 40; i < len - 32; i++) {
		bufSend[i] = 0;
	}
	int j = 8;
	for (int i = len - 32; i < len - 24; i++) {
		bufSend[i] = buf[j];
		j++;
	}
	int iSndRetval;
	U8* buf1 = (U8*)malloc((len - 24) / 8);
	iSndRetval = BitArrayToByteArray(bufSend, (len - 24), buf1, (len - 24) / 8);
	iSndRetval = SendtoLower(buf1, iSndRetval, port);
	//printByteBuf(buf1, iSndRetval);
	//ͳ��
	if (iSndRetval <= 0) {
		iSndErrorCount++;
	}
	else {
		iRcvForward += iSndRetval;
		iRcvForwardCount++;
	}
}
//�����ϲ�
void send2up(U8* buf, int len, int ifNo) {
	U8* bufSend = (U8*)malloc(len - 40);
	int iSndRetval;
	for (int i = 24; i < len - 16; i++)
		bufSend[i - 24] = buf[i];
	free(buf);
	U8* buf1 = (U8*)malloc((len - 40) / 8);
	int leng = BitArrayToByteArray(bufSend, len - 40, buf1, (len - 40) / 8);
	iSndRetval = SendtoUpper(buf1, leng);
	//printByteBuf(buf1, leng);
	iSndRetval = iSndRetval * 8;//�����λ������ͳ��
	//ͳ��
	if (iSndRetval <= 0) {
		iSndErrorCount++;
	}
	else {
		iRcvToUpper += iSndRetval;
		iRcvToUpperCount++;
	}
}
//����Դ
void updateSource(U8* buf) {
	int source = string2int(strDevID) - 1;
	int i = 11;
	while (source) {
		buf[i] = source % 2;
		source /= 2;
		i--;
	}
	if (source == 0) {
		buf[9] = 0;
		buf[10] = 0;
		buf[11] = 0;
	}
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
void RecvfromLower(U8* buf, int len, int ifNo){
	int getPort = 0;
	int leng = 0;
	U8* bufsend = (U8*)malloc(len * 8);
	leng = ByteArrayToBitArray(bufsend, len * 8, buf, len);
	//��Ϊ·����
	if (lowerNumber > 1) {
		//�ӽӿ�0�յ������ݣ����
		getPort = findPort(bufsend);
		printf("\nport: %d\n", getPort);
		//printBuf(bufsend);
		//printRoutingTable();
		//����port������
		if (getPort >= 0) {
			if (bufsend[8]) {
				printf("Sending to self\n");
				send2up(bufsend, leng, ifNo);
			}
			else if (getPort != ifNo) {
				printf("Sending to others\n");
				//���µ�ַ
				updateSource(bufsend);
				//printBuf(bufsend);
				send2low(bufsend, getLen(bufsend), getPort);
			}
			
		}
		//-1��ʾ����·��������
		else if (getPort == -1) {
			printf("Sending to self\n");
			send2up(bufsend, leng, ifNo);
		}
	}
	else {
		send2up(bufsend, leng, ifNo);
	}
	switch (iWorkMode % 10) {
	case 1:
		cout <<endl<< "���սӿ� " << ifNo << " ���ݣ�" << endl;
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
	cout << endl << endl << "�豸��:" << strDevID << ",    ���:" << strLayer << ",    ʵ���:" << strEntity ;
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