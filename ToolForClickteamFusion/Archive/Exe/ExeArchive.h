#pragma once

#include "../Archive.h"

namespace CTT {

	namespace Archive {

		typedef struct _archive_dll_info 
		{
			wstring name;
			UINT32 checksum;
			BYTE* data;
			UINT32 size;
		} Module;

		class ExeArchive : public Archive 
		{
		public:
			ExeArchive(wstring archivePath);
			~ExeArchive();
			vector<Module>& getModules() { return modules; }
			bool init();
			bool makeModuleReplacedArchive();
			bool writeModulesIntoArchive(HANDLE hArchive);
			bool exportModules();
			bool importModules();
		private:
			UINT32 frontHeaderStartOffset;		// module 관련 정보를 얻기 위함
			vector<Module> modules;
			bool readModulesFromArchive();
		};
	
	}

}