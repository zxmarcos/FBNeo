// The texts of history.xml and command.dat for a driver, without any UI
// (the parsers of win32/gameinfo.cpp, giving UTF-8 text instead of RTF)
#ifndef GAMEINFO_TEXT_H
#define GAMEINFO_TEXT_H

// UTF-8, the lines end with '\n'. The first line is the title of the entry,
// the lines that start with "- " are the headings of the sections.
// NULL if the file or the entry wasn't found, free() the text otherwise.
// They use nBurnDrvActive for a while, so nothing else (a rom scan) may use it then.
char* GameInfoHistory(int nDrv);			// szAppHistoryPath "history.xml"
char* GameInfoCommands(int nDrv);			// szAppCommandPath "command.dat", the button and move codes expanded

#endif
