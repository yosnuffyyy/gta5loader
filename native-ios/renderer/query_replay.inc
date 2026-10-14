/* Query ID11165 is from the physical BEGIN_QUERY failure. Its CREATE type
   and following draw geometry were not supplied; test both required types
   with deterministic original-IM geometry through the actual ring protocol. */
#include "query_setup.inc"
enum {Q_BASIC=20,Q_STRESS=160,Q_TOTAL=Q_BASIC+Q_STRESS};
uint64_t expected[Q_TOTAL]={0};uint32_t sequences[Q_TOTAL];for(unsigned i=0;i<Q_TOTAL;i++){
 uint64_t address=dst+i*32;uint8_t guards[32];memset(guards,0xa5,sizeof(guards));host_store(h,address,guards,32);host_put32(h,address,0);
 sequences[i]=100+i;cmd[0]=11165+i;cmd[1]=i==3||i==4?5:1;cmd[2]=(uint32_t)address;cmd[3]=address>>32;EMIT(44,4);
}
/* Second color target and multisample attachments. */
for(unsigned t=0;t<3;t++){memset(cmd,0,sizeof(cmd));cmd[0]=118+t;cmd[1]=2;cmd[2]=cmd[3]=t==0?8:16;cmd[4]=cmd[5]=cmd[6]=1;cmd[7]=t==1?40:28;cmd[8]=t==1?0x40:0x28;cmd[9]=t?4:1;EMIT(3,10);}
uint32_t stencil_words[]={121,1,0,0,8,1,0xffff,1,1,1,1,1,1,1,1};memcpy(cmd,stencil_words,sizeof(stencil_words));EMIT(16,15);
memset(cmd,0,sizeof(cmd));cmd[0]=122;cmd[1]=2;cmd[2]=cmd[3]=16;cmd[4]=cmd[5]=cmd[6]=1;cmd[7]=45;cmd[8]=0x40;cmd[9]=1;EMIT(3,10);
#define QB(i) do{cmd[0]=11165+(i);EMIT(45,1);}while(0)
#define QE(i) do{cmd[0]=11165+(i);cmd[1]=sequences[(i)];EMIT(46,2);}while(0)
#define QD() do{memset(cmd,0,sizeof(cmd));cmd[1]=6;cmd[2]=1;EMIT(32,6);}while(0)
#define QT(color,depth) do{memset(cmd,0,sizeof(cmd));cmd[0]=(color);cmd[32]=(depth);EMIT(26,37);}while(0)
#define QDEPTH(id,z) do{memset(cmd,0,sizeof(cmd));cmd[0]=(id);cmd[3]=1;float value=(z);memcpy(cmd+4,&value,4);EMIT(6,6);}while(0)
#define QSTATE(id) do{cmd[0]=(id);cmd[1]=0;EMIT(28,2);}while(0)
#define QMASK(mask) do{memset(cmd,0,sizeof(cmd));for(unsigned j=1;j<5;j++)cmd[j]=0x3f800000;cmd[5]=(mask);EMIT(27,6);}while(0)
#define QZ(z) do{cmd[0]=111;cmd[1]=16+11*16+8;cmd[2]=4;float value=(z);memcpy(cmd+3,&value,4);EMIT(12,4);}while(0)
/* Count, multiple draws in one pass, empty query, visible/hidden predicate. */
QB(0);QD();QE(0);expected[0]=256;
QB(1);QD();QD();QE(1);expected[1]=512;
QB(2);QE(2);
QB(3);QD();QE(3);expected[3]=1;
QT(116,117);QDEPTH(117,.5f);QSTATE(107);QZ(.75f);QB(4);QD();QE(4);
QB(5);QD();QE(5);QZ(.25f);QDEPTH(117,.5f);QB(6);QD();QE(6);expected[6]=256;
QSTATE(0);QT(116,0);QZ(0);
/* Scissor rejects/crops real fragments; no fabricated visible threshold. */
cmd[0]=105;EMIT(29,1);cmd[0]=1;cmd[1]=2;cmd[2]=3;cmd[3]=10;cmd[4]=11;EMIT(31,5);QB(7);QD();QE(7);expected[7]=64;cmd[0]=104;EMIT(29,1);
/* One logical query spans render-target dimensions and queue submissions. */
QB(8);QD();QT(118,0);QD();QE(8);expected[8]=320;QT(116,0);
QB(9);QD();memset(cmd,0,sizeof(cmd));cmd[0]=112;cmd[1]=63*4;cmd[2]=4;float one=1;memcpy(cmd+3,&one,4);EMIT(12,4);QD();QE(9);expected[9]=512;
/* Overlapping independent scopes receive only their own draw intervals. */
QB(10);QD();QB(11);QD();QE(11);QD();QE(10);expected[10]=768;expected[11]=256;
/* Reuse before submission cancels the old generation's visibility result. */
QB(12);QD();cmd[0]=11177;cmd[1]=999;EMIT(46,2);cmd[0]=105;EMIT(29,1);memset(cmd,0,sizeof(cmd));cmd[0]=1;EMIT(31,5);QB(12);QD();QE(12);cmd[0]=104;EMIT(29,1);
/* MSAA counts samples, including a two-sample mask and total rejection. */
QT(120,119);QDEPTH(119,.5f);QB(13);QD();QE(13);expected[13]=1024;
QMASK(5);QB(14);QD();QE(14);expected[14]=512;QMASK(0);QB(15);QD();QE(15);
QMASK(UINT32_MAX);QT(116,122);QSTATE(121);memset(cmd,0,sizeof(cmd));cmd[0]=122;cmd[3]=3;float half=.5f;memcpy(cmd+4,&half,4);cmd[5]=7;EMIT(6,6);QB(16);QD();QE(16);QSTATE(0);QT(116,0);
/* Depth-only draws with no fragment shader or color writes. */
cmd[0]=1;cmd[1]=0;EMIT(18,2);QT(0,117);QDEPTH(117,.5f);QSTATE(107);QB(17);QD();QE(17);expected[17]=256;cmd[0]=1;cmd[1]=102;EMIT(18,2);QSTATE(0);QT(116,0);
/* Destroy a pending generation, reuse the same ID with a different address. */
uint64_t old_address=dst+6400;host_put32(h,old_address,0);cmd[0]=13000;cmd[1]=1;cmd[2]=(uint32_t)old_address;cmd[3]=old_address>>32;EMIT(44,4);cmd[0]=13000;EMIT(45,1);QD();cmd[0]=13000;cmd[1]=800;EMIT(46,2);cmd[0]=13000;EMIT(4,1);
cmd[0]=13000;cmd[1]=1;cmd[2]=(uint32_t)(dst+18*32);cmd[3]=(dst+18*32)>>32;EMIT(44,4);cmd[0]=13000;EMIT(45,1);QD();cmd[0]=13000;cmd[1]=sequences[18];EMIT(46,2);expected[18]=256;
/* An active destroyed query must never publish a completion afterward. */
QB(19);QD();cmd[0]=11184;EMIT(4,1);sequences[19]=0;
/* Enough segments to exercise full-page flush/reuse and bounded mappings. */
for(unsigned t=Q_BASIC;t<Q_TOTAL;t++){QB(t);QD();QE(t);expected[t]=256;}
#undef QB
#undef QE
#undef QD
#undef QT
#undef QDEPTH
#undef QSTATE
#undef QMASK
#undef QZ
