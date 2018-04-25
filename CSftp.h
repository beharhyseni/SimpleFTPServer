//
// Created by cathy on 2018-03-17.
//

#ifndef A3_M4M0B_R6P0B_CSFTP_H
#define A3_M4M0B_R6P0B_CSFTP_H

bool runCommand(int, char*);
bool checkArguments(char*, int);
void runUserCmd(char*, int);
void runCwdCmd(char*, int);
void runCDUPCmd(int);
void runTypeCmd(char*, int);
void runModeCmd(char*, int);
void runStruCmd(char*, int);
void runRetrCmd(char*, int);
void runPasvCmd(int);
void runNlstCmd(int);

#endif //A3_M4M0B_R6P0B_CSFTP_H
