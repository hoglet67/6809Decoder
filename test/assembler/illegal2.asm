FOR I%=&10 TO &13 STEP 3P%=&3000[OPT I%.testLDMD #&01STS &0080LDX &0232STX oldvecLDX #loop2STX &0232.loop1LDX #illegal.loop2LDD ,X++STD instrLDS &0080.instrNOPNOPEQUB &00:INC instr+2BNE loop1LDX oldvecSTX &0232LDS &0080RTS:.oldvecEQUW &0000:.illegalEQUW &1215EQUW &1218EQUW &121BEQUW &1238EQUW &123EEQUW &1241EQUW &1242EQUW &1245EQUW &124BEQUW &124EEQUW &1251EQUW &1252EQUW &1255EQUW &125BEQUW &125EEQUW &1287EQUW &128FEQUW &12C7EQUW &12CFEQUW &1000EQUW &1001EQUW &1002EQUW &1003EQUW &1004EQUW &1005EQUW &1006EQUW &1007EQUW &1008EQUW &1009EQUW &100AEQUW &100BEQUW &100CEQUW &100DEQUW &100EEQUW &100FEQUW &1010EQUW &1011EQUW &1012EQUW &1013EQUW &1014EQUW &1015EQUW &1016EQUW &1017EQUW &1018EQUW &1019EQUW &101AEQUW &101BEQUW &101CEQUW &101DEQUW &101EEQUW &101FEQUW &1020EQUW &103CEQUW &103DEQUW &103EEQUW &1041EQUW &1042EQUW &1045EQUW &104BEQUW &104EEQUW &1050EQUW &1051EQUW &1052EQUW &1055EQUW &1057EQUW &1058EQUW &105BEQUW &105EEQUW &1060EQUW &1061EQUW &1062EQUW &1063EQUW &1064EQUW &1065EQUW &1066EQUW &1067EQUW &1068EQUW &1069EQUW &106AEQUW &106BEQUW &106CEQUW &106DEQUW &106EEQUW &106FEQUW &1070EQUW &1071EQUW &1072EQUW &1073EQUW &1074EQUW &1075EQUW &1076EQUW &1077EQUW &1078EQUW &1079EQUW &107AEQUW &107BEQUW &107CEQUW &107DEQUW &107EEQUW &107FEQUW &1087EQUW &108DEQUW &108FEQUW &109DEQUW &10ADEQUW &10BDEQUW &10C0EQUW &10C1EQUW &10C2EQUW &10C3EQUW &10C4EQUW &10C5EQUW &10C6EQUW &10C7EQUW &10C8EQUW &10C9EQUW &10CAEQUW &10CBEQUW &10CCEQUW &10CDEQUW &10CFEQUW &10D0EQUW &10D1EQUW &10D2EQUW &10D3EQUW &10D4EQUW &10D5EQUW &10D6EQUW &10D7EQUW &10D8EQUW &10D9EQUW &10DAEQUW &10DBEQUW &10E0EQUW &10E1EQUW &10E2EQUW &10E3EQUW &10E4EQUW &10E5EQUW &10E6EQUW &10E7EQUW &10E8EQUW &10E9EQUW &10EAEQUW &10EBEQUW &10F0EQUW &10F1EQUW &10F2EQUW &10F3EQUW &10F4EQUW &10F5EQUW &10F6EQUW &10F7EQUW &10F8EQUW &10F9EQUW &10FAEQUW &10FBEQUW &1100EQUW &1101EQUW &1102EQUW &1103EQUW &1104EQUW &1105EQUW &1106EQUW &1107EQUW &1108EQUW &1109EQUW &110AEQUW &110BEQUW &110CEQUW &110DEQUW &110EEQUW &110FEQUW &1110EQUW &1111EQUW &1112EQUW &1113EQUW &1114EQUW &1115EQUW &1116EQUW &1117EQUW &1118EQUW &1119EQUW &111AEQUW &111BEQUW &111CEQUW &111DEQUW &111EEQUW &111FEQUW &1120EQUW &1121EQUW &1122EQUW &1123EQUW &1124EQUW &1125EQUW &1126EQUW &1127EQUW &1128EQUW &1129EQUW &112AEQUW &112BEQUW &112CEQUW &112DEQUW &112EEQUW &112FEQUW &113EEQUW &1140EQUW &1141EQUW &1142EQUW &1144EQUW &1145EQUW &1146EQUW &1147EQUW &1148EQUW &1149EQUW &114BEQUW &114EEQUW &1150EQUW &1151EQUW &1152EQUW &1154EQUW &1155EQUW &1156EQUW &1157EQUW &1158EQUW &1159EQUW &115BEQUW &115EEQUW &1160EQUW &1161EQUW &1162EQUW &1163EQUW &1164EQUW &1165EQUW &1166EQUW &1167EQUW &1168EQUW &1169EQUW &116AEQUW &116BEQUW &116CEQUW &116DEQUW &116EEQUW &116FEQUW &1170EQUW &1171EQUW &1172EQUW &1173EQUW &1174EQUW &1175EQUW &1176EQUW &1177EQUW &1178EQUW &1179EQUW &117AEQUW &117BEQUW &117CEQUW &117DEQUW &117EEQUW &117FEQUW &1182EQUW &1184EQUW &1185EQUW &1187EQUW &1188EQUW &1189EQUW &118AEQUW &1192EQUW &1194EQUW &1195EQUW &1198EQUW &1199EQUW &119AEQUW &11A2EQUW &11A4EQUW &11A5EQUW &11A8EQUW &11A9EQUW &11AAEQUW &11B2EQUW &11B4EQUW &11B5EQUW &11B8EQUW &11B9EQUW &11BAEQUW &11C2EQUW &11C3EQUW &11C4EQUW &11C5EQUW &11C7EQUW &11C8EQUW &11C9EQUW &11CAEQUW &11CCEQUW &11CDEQUW &11CEEQUW &11CFEQUW &11D2EQUW &11D3EQUW &11D4EQUW &11D5EQUW &11D8EQUW &11D9EQUW &11DAEQUW &11DCEQUW &11DDEQUW &11DEEQUW &11DFEQUW &11E2EQUW &11E3EQUW &11E4EQUW &11E5EQUW &11E8EQUW &11E9EQUW &11EAEQUW &11ECEQUW &11EDEQUW &11EEEQUW &11EFEQUW &11F2EQUW &11F3EQUW &11F4EQUW &11F5EQUW &11F8EQUW &11F9EQUW &11FAEQUW &11FCEQUW &11FDEQUW &11FEEQUW &11FFBRA P%+3]NEXTZ=GETCALL test