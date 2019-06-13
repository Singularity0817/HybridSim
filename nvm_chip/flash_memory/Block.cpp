#include "Block.h"

namespace NVM
{
	namespace FlashMemory
	{
		Block::Block(unsigned int PagesNoPerBlock, flash_block_ID_type BlockID)
		{
			ID = BlockID;
			Pages = new Page[PagesNoPerBlock];
		}

		//*ZWH*
		Block::Block(unsigned int PagesNoPerBlock, flash_block_ID_type BlockID, Flash_Technology_Type flash_tech_type)
		{
			ID = BlockID;
			Pages = new Page[PagesNoPerBlock];
			flash_type = flash_tech_type;
		}

		Block::~Block()
		{
		delete[] Pages;
		}
	}
}