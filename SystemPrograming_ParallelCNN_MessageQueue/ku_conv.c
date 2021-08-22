#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <unistd.h>

void makeMatrix(int** matrix, int X, int Y);
void maxPooling(int** matrix,int** aftermat,int row, int mqdes1,int mqdes2);
void conv(int** matrix,int** filter,int** aftermat,int row,int mqdes1,int mqdes2);
int convCal(int** matrix,int** filter,int i,int j);
int poolCal(int** matrix,int i,int j);
int** makeAllocation(int** matrix, int row);
void freeAllocation(int** matrix, int row);


typedef struct Msgsnd{
	long id;
	int* value;
}Msgsnd;

typedef struct Msgrcv{
	long id;
	int value;
}Msgrcv;

int main(int argc, char** argv){
	
	int row = atoi(argv[1]);
	int col = row;
	int mqdes1,mqdes2;
	key_t ipckey1;
	key_t ipckey2;

	ipckey1 = ftok("./",1000);
	mqdes1 = msgget(ipckey1, IPC_CREAT| 0600);
	if(mqdes1<0){
		perror("msgget()");
		exit(0);
	}

	ipckey2 = ftok("./",2000);
	mqdes2 = msgget(ipckey2, IPC_CREAT| 0600);
	if(mqdes2<0){
		perror("msgget()");
		exit(0);
	}

	int** layerMatrix;
	int** filter;
	int** afterConvmat;
	int** afterMaxpmat;
	layerMatrix=makeAllocation(layerMatrix,row);
	filter=makeAllocation(filter,3);
	afterConvmat=makeAllocation(afterConvmat,row-2);
	afterMaxpmat=makeAllocation(afterMaxpmat,(row-2)/2);

	makeMatrix(layerMatrix,row,col);

	filter[0][0]=-1;
	filter[0][1]=-1;
	filter[0][2]=-1;
	filter[1][0]=-1;
	filter[1][1]=8;
	filter[1][2]=-1;
	filter[2][0]=-1;
	filter[2][1]=-1;
	filter[2][2]=-1;

	conv(layerMatrix,filter,afterConvmat,row,mqdes1,mqdes2);
	maxPooling(afterConvmat,afterMaxpmat,(row-2),mqdes1,mqdes2);

	for(int i=0;i<(row-2)/2;i++){
		for(int j=0;j<(col-2)/2;j++){
			printf("%d ",afterMaxpmat[i][j]);
		}
	}

	freeAllocation(layerMatrix,row);
	freeAllocation(filter,3);
	freeAllocation(afterConvmat,row-2);
	freeAllocation(afterMaxpmat,(row-2)/2);

	msgctl(mqdes1,IPC_RMID,0);
	msgctl(mqdes2,IPC_RMID,0);

	return 0;
}


void maxPooling(int** matrix,int** aftermat,int row,int mqdes1,int mqdes2){
	
	int i,j;	
	pid_t pid;
	pid_t wpid;
	int child_status;
	int* index;
		
	Msgrcv mrcv;
	Msgsnd msnd;
	
	size_t msg_len =8;
	int count =0;
	int countTerm=0;
	int roof;
	index=(int*)malloc(sizeof(int)*2);
	
	for(i=0 ;i < row; ){
		for(j=0 ;j<row; ){
			while(1){
				count++;
				msnd.id=count;
				msnd.value=(int*)malloc(sizeof(int)*2);
				msnd.value[0]=i;
				msnd.value[1]=j;
				if(msgsnd(mqdes1,&msnd,msg_len,0)==-1){
					perror("msgsnd()");
					break;
				}
		
				if((pid=fork())==0){
					if(msgrcv(mqdes1,&msnd,msg_len,count,0)==-1){
						perror("msgrcv()");
						exit(0);
					}else{
						index=msnd.value;
					}
					aftermat[index[0]/2][index[1]/2]=poolCal(matrix,index[0],index[1]);
					mrcv.id=count;
					mrcv.value=aftermat[index[0]/2][index[1]/2];
					if(msgsnd(mqdes2,&mrcv,msg_len,0)==-1){
						perror("msgsnd()");
						break;
					}
			
				exit(0);
				}
			
				j=j+2;
				if(count%4==0||j==row){
					break;
				}
			}

			if(count%4==0){
				countTerm++;	
				for(roof=1;roof<=4;roof++){
					wpid=wait(&child_status);
			
					if(msgrcv(mqdes2,&mrcv,msg_len,roof+(countTerm-1)*4,0)==-1){
						perror("msgrcv()");
						exit(0);
					}else{
						aftermat[(mrcv.id-1)/(row/2)][(mrcv.id-1)%(row/2)]=mrcv.value;
					}
				}
			}

			if(count==(row/2)*(row/2) && (row/2)*(row/2)%2==1){
				wpid=wait(&child_status);
				
					if(msgrcv(mqdes2,&mrcv,msg_len,count,0)==-1){
						perror("msgrcv()");
						exit(0);
					}else{
						aftermat[(mrcv.id-1)/(row/2)][(mrcv.id-1)%(row/2)]=mrcv.value;
					}
			}
		}
		i=i+2;
	}

};

void conv(int** matrix, int** filter,int** aftermat,int row,int mqdes1,int mqdes2){
	int i,j;
	pid_t pid;
	pid_t wpid;
	int child_status;
	int* index;
	int aftervalue;
	Msgrcv mrcv;
	Msgsnd msnd;
	size_t msg_len =8;
	int count =0;
	int countTerm=0;
	int roof;	

	index=(int*)malloc(sizeof(int)*2);
	
	for(i=0 ;i < row-2;i++){
		for(j=0 ;j<row-2;){
			while(1){
			count++;
			msnd.id=count;
			msnd.value=(int*)malloc(sizeof(int)*2);
			msnd.value[0]=i;
			msnd.value[1]=j;


			if(msgsnd(mqdes1,&msnd,msg_len,0)==-1){
				perror("msgsnd()");
				break;
			}
	
			if((pid=fork())==0){
				if(msgrcv(mqdes1,&msnd,msg_len,count,0)==-1){
					perror("msgrcv()");
					exit(9999);
				}else{
					index=msnd.value;
				}

				aftervalue=convCal(matrix,filter,index[0],index[1]);
				mrcv.id=count;
				mrcv.value=aftervalue;
				if(msgsnd(mqdes2,&mrcv,msg_len,0)==-1){
					perror("msgsnd()");
					break;
				}

				exit(count);
			}
				j++;
				if(count%4==0||j==row-2){
					break;
				}
			}

			if(count%4==0){
				countTerm++;	
				for(roof=1;roof<=4;roof++){
					wpid=wait(&child_status);
			
					if(msgrcv(mqdes2,&mrcv,msg_len,roof+(countTerm-1)*4,0)==-1){
						perror("msgrcv()");
						exit(0);
					}else{
						aftermat[(mrcv.id-1)/(row-2)][(mrcv.id-1)%(row-2)]=mrcv.value;
					}
				}
			}
	
		}
	}

};	

int convCal(int** matrix,int** filter,int i,int j){
	int sum =0;
	int x,y;
	int f_x=0,f_y;
	for(x=i;x<i+3;x++,f_x++){
		f_y=0;
		for(y=j;y<j+3;y++,f_y++){
			sum+= matrix[x][y]*filter[f_x][f_y];
		}
	}

	return sum;
};

int poolCal(int** matrix,int i,int j){
	
	int temp=matrix[i][j];
	int x,y;
	for(x=i;x<i+2;x++){
		for(y=j;y<j+2;y++){
			if(temp<matrix[x][y]){
				temp=matrix[x][y];
			}
		}
	}
	return temp;
};

int** makeAllocation(int** matrix,int row){

	int i=0;
	matrix=(int**)malloc(sizeof(int*)*row);
	for(i;i<row;i++){
		matrix[i]=(int*)malloc(sizeof(int)*row);
	}

	return matrix;
}


void freeAllocation(int** matrix, int row){

	for(int i=0;i<row;i++){
		free(matrix[i]);
	}
	free(matrix);
}



