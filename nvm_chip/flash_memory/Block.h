#ifndef BLOCK_H
#define BLOCK_H

#include "FlashTypes.h"
#include "Page.h"


namespace NVM
{
	namespace FlashMemory
	{
		class Block
		{
		public:
			Block(unsigned int PagesNoPerBlock, flash_block_ID_type BlockID);
			Block(unsigned int PagesNoPerBlock, flash_block_ID_type BlockID, Flash_Technology_Type flash_tech_type);
			~Block();
			Page* Pages;						//Records the status of each sub-page
			flash_block_ID_type ID;            //Again this variable is required in list based garbage collections
			//BlockMetadata Metadata;
			Flash_Technology_Type flash_type;   //*ZWH*
		};
	}
}
#endif // ! BLOCK_H
