#include "Flash_Block_Manager.h"


namespace SSD_Components
{
	unsigned int Block_Pool_Slot_Type::Page_vector_size = 0;
	Flash_Block_Manager_Base::Flash_Block_Manager_Base(GC_and_WL_Unit_Base* gc_and_wl_unit, unsigned int max_allowed_block_erase_count, unsigned int total_concurrent_streams_no,
		unsigned int channel_count, unsigned int chip_no_per_channel, unsigned int die_no_per_chip, unsigned int plane_no_per_die,
		unsigned int block_no_per_plane, unsigned int page_no_per_block)
		: gc_and_wl_unit(gc_and_wl_unit), max_allowed_block_erase_count(max_allowed_block_erase_count), total_concurrent_streams_no(total_concurrent_streams_no),
		channel_count(channel_count), chip_no_per_channel(chip_no_per_channel), die_no_per_chip(die_no_per_chip), plane_no_per_die(plane_no_per_die),
		block_no_per_plane(block_no_per_plane), pages_no_per_block(page_no_per_block)
	{
		plane_manager = new PlaneBookKeepingType***[channel_count];
		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
		{
			plane_manager[channelID] = new PlaneBookKeepingType**[chip_no_per_channel];
			for (unsigned int chipID = 0; chipID < chip_no_per_channel; chipID++)
			{
				plane_manager[channelID][chipID] = new PlaneBookKeepingType*[die_no_per_chip];
				for (unsigned int dieID = 0; dieID < die_no_per_chip; dieID++)
				{
					plane_manager[channelID][chipID][dieID] = new PlaneBookKeepingType[plane_no_per_die];
					for (unsigned int planeID = 0; planeID < plane_no_per_die; planeID++)//Initialize plane book keeping data structure
					{
						plane_manager[channelID][chipID][dieID][planeID].Total_pages_count = block_no_per_plane * pages_no_per_block;
						plane_manager[channelID][chipID][dieID][planeID].Free_pages_count = block_no_per_plane * pages_no_per_block;
						plane_manager[channelID][chipID][dieID][planeID].Valid_pages_count = 0;
						plane_manager[channelID][chipID][dieID][planeID].Invalid_pages_count = 0;
						plane_manager[channelID][chipID][dieID][planeID].Ongoing_erase_operations.clear();
						plane_manager[channelID][chipID][dieID][planeID].Ongoing_slc_erase_operations.clear();
						plane_manager[channelID][chipID][dieID][planeID].Blocks = new Block_Pool_Slot_Type[block_no_per_plane];
						for (unsigned int blockID = 0; blockID < block_no_per_plane; blockID++)//Initialize block pool for plane
						{
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].BlockID = blockID;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Current_page_write_index = 0;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Current_status = Block_Service_Status::IDLE;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Invalid_page_count = 0;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Erase_count = 0;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Holds_mapping_data = false;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Has_ongoing_gc_wl = false;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Erase_transaction = NULL;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Ongoing_user_program_count = 0;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Ongoing_user_read_count = 0;
							Block_Pool_Slot_Type::Page_vector_size = pages_no_per_block / (sizeof(uint64_t) * 8) + (pages_no_per_block % (sizeof(uint64_t) * 8) == 0 ? 0 : 1);
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Invalid_page_bitmap = new uint64_t[Block_Pool_Slot_Type::Page_vector_size];
							for (unsigned int i = 0; i < Block_Pool_Slot_Type::Page_vector_size; i++)
								plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Invalid_page_bitmap[i] = All_VALID_PAGE;
							plane_manager[channelID][chipID][dieID][planeID].Add_to_free_block_pool(&plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID], false);
						}
						plane_manager[channelID][chipID][dieID][planeID].Data_wf = new Block_Pool_Slot_Type*[total_concurrent_streams_no];
						plane_manager[channelID][chipID][dieID][planeID].Translation_wf = new Block_Pool_Slot_Type*[total_concurrent_streams_no];
						plane_manager[channelID][chipID][dieID][planeID].GC_wf = new Block_Pool_Slot_Type*[total_concurrent_streams_no];
						for (unsigned int stream_cntr = 0; stream_cntr < total_concurrent_streams_no; stream_cntr++)
						{
							plane_manager[channelID][chipID][dieID][planeID].Data_wf[stream_cntr] = plane_manager[channelID][chipID][dieID][planeID].Get_a_free_block(stream_cntr, false);
							plane_manager[channelID][chipID][dieID][planeID].Translation_wf[stream_cntr] = plane_manager[channelID][chipID][dieID][planeID].Get_a_free_block(stream_cntr, true);
							plane_manager[channelID][chipID][dieID][planeID].GC_wf[stream_cntr] = plane_manager[channelID][chipID][dieID][planeID].Get_a_free_block(stream_cntr, false);
						}
					}
				}
			}
		}
	}

	Flash_Block_Manager_Base::Flash_Block_Manager_Base(GC_and_WL_Unit_Base* gc_and_wl_unit, unsigned int max_allowed_block_erase_count, unsigned int total_concurrent_streams_no,
		unsigned int channel_count, unsigned int chip_no_per_channel, unsigned int die_no_per_chip, unsigned int plane_no_per_die,
		unsigned int block_no_per_plane, unsigned int page_no_per_block, unsigned int slc_block_no_per_plane, unsigned int page_no_per_slc_block)
		: gc_and_wl_unit(gc_and_wl_unit), max_allowed_block_erase_count(max_allowed_block_erase_count), total_concurrent_streams_no(total_concurrent_streams_no),
		channel_count(channel_count), chip_no_per_channel(chip_no_per_channel), die_no_per_chip(die_no_per_chip), plane_no_per_die(plane_no_per_die),
		block_no_per_plane(block_no_per_plane), pages_no_per_block(page_no_per_block), slc_block_no_per_plane(slc_block_no_per_plane), page_no_per_slc_block(page_no_per_slc_block)
	{
		tlc_block_no_per_plane = block_no_per_plane - slc_block_no_per_plane;
		std::cout << "Initializing Flash_Block_Manager that supports SLC-TLC combined SSD " << slc_block_no_per_plane << ":" << tlc_block_no_per_plane << std::endl;
		page_no_per_tlc_block = page_no_per_block;
		plane_manager = new PlaneBookKeepingType***[channel_count];
		Max_data_migration_trigger_one_time = 10;
		for (unsigned int channelID = 0; channelID < channel_count; channelID++)
		{
			plane_manager[channelID] = new PlaneBookKeepingType**[chip_no_per_channel];
			for (unsigned int chipID = 0; chipID < chip_no_per_channel; chipID++)
			{
				plane_manager[channelID][chipID] = new PlaneBookKeepingType*[die_no_per_chip];
				for (unsigned int dieID = 0; dieID < die_no_per_chip; dieID++)
				{
					plane_manager[channelID][chipID][dieID] = new PlaneBookKeepingType[plane_no_per_die];
					for (unsigned int planeID = 0; planeID < plane_no_per_die; planeID++)//Initialize plane book keeping data structure
					{
						//*ZWH* in an SLC-TLC combined SSD, SLC blocks and TLC blocks are managed seperately!
						//SLC block records
						plane_manager[channelID][chipID][dieID][planeID].Total_slc_pages_count = slc_block_no_per_plane * page_no_per_slc_block;
						plane_manager[channelID][chipID][dieID][planeID].Free_slc_pages_count = slc_block_no_per_plane * page_no_per_slc_block;
						plane_manager[channelID][chipID][dieID][planeID].Valid_slc_pages_count = 0;
						plane_manager[channelID][chipID][dieID][planeID].Invalid_slc_pages_count = 0;
						//TLC block records
						plane_manager[channelID][chipID][dieID][planeID].Total_tlc_pages_count = tlc_block_no_per_plane * page_no_per_tlc_block;
						plane_manager[channelID][chipID][dieID][planeID].Free_tlc_pages_count = tlc_block_no_per_plane * page_no_per_tlc_block;
						plane_manager[channelID][chipID][dieID][planeID].Valid_tlc_pages_count = 0;
						plane_manager[channelID][chipID][dieID][planeID].Invalid_tlc_pages_count = 0;
						//Total block records
						plane_manager[channelID][chipID][dieID][planeID].Total_pages_count = slc_block_no_per_plane * page_no_per_slc_block + tlc_block_no_per_plane * page_no_per_tlc_block;
						plane_manager[channelID][chipID][dieID][planeID].Free_pages_count = slc_block_no_per_plane * page_no_per_slc_block + tlc_block_no_per_plane * page_no_per_tlc_block;
						plane_manager[channelID][chipID][dieID][planeID].Valid_pages_count = 0;
						plane_manager[channelID][chipID][dieID][planeID].Invalid_pages_count = 0;
						plane_manager[channelID][chipID][dieID][planeID].On_going_slc_transaction_num = 0;
						plane_manager[channelID][chipID][dieID][planeID].On_going_tlc_transaction_num = 0;
						plane_manager[channelID][chipID][dieID][planeID].Triggered_data_migration = 0;
						plane_manager[channelID][chipID][dieID][planeID].Completed_data_migration = 0;
						plane_manager[channelID][chipID][dieID][planeID].Doing_data_migration = false;
						plane_manager[channelID][chipID][dieID][planeID].Data_migration_should_be_terminated = false;
						plane_manager[channelID][chipID][dieID][planeID].Doing_garbage_collection = false;
						plane_manager[channelID][chipID][dieID][planeID].Triggered_garbage_collection = 0;
						plane_manager[channelID][chipID][dieID][planeID].Completed_garbage_collection = 0;
						/*
						plane_manager[channelID][chipID][dieID][planeID].Total_pages_count = block_no_per_plane * pages_no_per_block;
						plane_manager[channelID][chipID][dieID][planeID].Free_pages_count = block_no_per_plane * pages_no_per_block;
						plane_manager[channelID][chipID][dieID][planeID].Valid_pages_count = 0;
						plane_manager[channelID][chipID][dieID][planeID].Invalid_pages_count = 0;
						*/
						plane_manager[channelID][chipID][dieID][planeID].Ongoing_erase_operations.clear();
						plane_manager[channelID][chipID][dieID][planeID].Ongoing_slc_erase_operations.clear();
						plane_manager[channelID][chipID][dieID][planeID].Blocks = new Block_Pool_Slot_Type[block_no_per_plane];
						
						//*ZWH* initialize SLC blocks first
						for (unsigned int blockID = 0; blockID < slc_block_no_per_plane; blockID++)//Initialize block pool for plane
						{
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].BlockID = blockID;
							//*ZWH*
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Flash_type = Flash_Technology_Type::SLC;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Pages_count = page_no_per_slc_block;
							//*ZWH*
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Current_page_write_index = 0;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Current_status = Block_Service_Status::IDLE;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Invalid_page_count = 0;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Erase_count = 0;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Holds_mapping_data = false;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Has_ongoing_gc_wl = false;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Erase_transaction = NULL;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Ongoing_user_program_count = 0;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Ongoing_user_read_count = 0;
							Block_Pool_Slot_Type::Page_vector_size = pages_no_per_block / (sizeof(uint64_t) * 8) + (pages_no_per_block % (sizeof(uint64_t) * 8) == 0 ? 0 : 1);
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Invalid_page_bitmap = new uint64_t[Block_Pool_Slot_Type::Page_vector_size];
							for (unsigned int i = 0; i < Block_Pool_Slot_Type::Page_vector_size; i++)
								plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Invalid_page_bitmap[i] = All_VALID_PAGE;
							plane_manager[channelID][chipID][dieID][planeID].Add_to_free_slc_block_pool(&plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID], false);
						}
						//*ZWH* then initialize TLC blocks
						for (unsigned int blockID = slc_block_no_per_plane; blockID < block_no_per_plane; blockID++)//Initialize block pool for plane
						{
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].BlockID = blockID;
							//*ZWH*
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Flash_type = Flash_Technology_Type::TLC;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Pages_count = page_no_per_tlc_block;
							//*ZWH*
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Current_page_write_index = 0;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Current_status = Block_Service_Status::IDLE;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Invalid_page_count = 0;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Erase_count = 0;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Holds_mapping_data = false;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Has_ongoing_gc_wl = false;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Erase_transaction = NULL;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Ongoing_user_program_count = 0;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Ongoing_user_read_count = 0;
							Block_Pool_Slot_Type::Page_vector_size = pages_no_per_block / (sizeof(uint64_t) * 8) + (pages_no_per_block % (sizeof(uint64_t) * 8) == 0 ? 0 : 1);
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Invalid_page_bitmap = new uint64_t[Block_Pool_Slot_Type::Page_vector_size];
							for (unsigned int i = 0; i < Block_Pool_Slot_Type::Page_vector_size; i++)
								plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Invalid_page_bitmap[i] = All_VALID_PAGE;
							plane_manager[channelID][chipID][dieID][planeID].Add_to_free_tlc_block_pool(&plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID], false);
						}
						plane_manager[channelID][chipID][dieID][planeID].Data_wf = new Block_Pool_Slot_Type*[total_concurrent_streams_no];
						plane_manager[channelID][chipID][dieID][planeID].Translation_wf = new Block_Pool_Slot_Type*[total_concurrent_streams_no];
						plane_manager[channelID][chipID][dieID][planeID].GC_wf = new Block_Pool_Slot_Type*[total_concurrent_streams_no];
						//*ZWH*
						plane_manager[channelID][chipID][dieID][planeID].Data_slc_wf = new Block_Pool_Slot_Type*[total_concurrent_streams_no];
						plane_manager[channelID][chipID][dieID][planeID].Data_tlc_wf = new Block_Pool_Slot_Type*[total_concurrent_streams_no];
						//*ZWH*
						for (unsigned int stream_cntr = 0; stream_cntr < total_concurrent_streams_no; stream_cntr++)
						{
							plane_manager[channelID][chipID][dieID][planeID].Translation_wf[stream_cntr] = plane_manager[channelID][chipID][dieID][planeID].Get_a_free_tlc_block(stream_cntr, true);
							plane_manager[channelID][chipID][dieID][planeID].Data_slc_wf[stream_cntr] = plane_manager[channelID][chipID][dieID][planeID].Get_a_free_slc_block(stream_cntr, false);
							plane_manager[channelID][chipID][dieID][planeID].Data_tlc_wf[stream_cntr] = plane_manager[channelID][chipID][dieID][planeID].Get_a_free_tlc_block(stream_cntr, false);
							plane_manager[channelID][chipID][dieID][planeID].GC_wf[stream_cntr] = plane_manager[channelID][chipID][dieID][planeID].Get_a_free_tlc_block(stream_cntr, false);
						}
					}
				}
			}
		}
	}

	Flash_Block_Manager_Base::~Flash_Block_Manager_Base() 
	{
		for (unsigned int channel_id = 0; channel_id < channel_count; channel_id++)
		{
			for (unsigned int chip_id = 0; chip_id < chip_no_per_channel; chip_id++)
			{
				for (unsigned int die_id = 0; die_id < die_no_per_chip; die_id++)
				{
					for (unsigned int plane_id = 0; plane_id < plane_no_per_die; plane_id++)
					{
						for (unsigned int blockID = 0; blockID < block_no_per_plane; blockID++)
							delete[] plane_manager[channel_id][chip_id][die_id][plane_id].Blocks[blockID].Invalid_page_bitmap;
						delete[] plane_manager[channel_id][chip_id][die_id][plane_id].Blocks;
						delete[] plane_manager[channel_id][chip_id][die_id][plane_id].GC_wf;
						delete[] plane_manager[channel_id][chip_id][die_id][plane_id].Data_wf;
						delete[] plane_manager[channel_id][chip_id][die_id][plane_id].Translation_wf;
					}
					delete[] plane_manager[channel_id][chip_id][die_id];
				}
				delete[] plane_manager[channel_id][chip_id];
			}
			delete[] plane_manager[channel_id];
		}
		delete[] plane_manager;
	}

	void Flash_Block_Manager_Base::Set_GC_and_WL_Unit(GC_and_WL_Unit_Base* gcwl) { this->gc_and_wl_unit = gcwl; }

	void Block_Pool_Slot_Type::Erase()
	{
		if (Invalid_page_count != Pages_count)
		{
			std::cout << "Erasing a block with VALID Page!!!" << std::endl;
			std::cin.get();
		}
		Current_page_write_index = 0;
		Invalid_page_count = 0;
		Erase_count++;
		for (unsigned int i = 0; i < Block_Pool_Slot_Type::Page_vector_size; i++)
			Invalid_page_bitmap[i] = All_VALID_PAGE;
		Stream_id = NO_STREAM;
		Holds_mapping_data = false;
		Erase_transaction = NULL;
	}

	bool Block_Pool_Slot_Type::Check_Block_Metadata()
	{
		if (Flash_type == Flash_Technology_Type::SLC)
		{
			if (Pages_count > 128)
			{
				std::cout << "INVALID METADATA in block pool : Wrong flash type" << std::endl;
				std::cin.get();
				return false;
			}
		}
		if (Current_page_write_index > Pages_count)
		{
			std::cout << "INVALID METADATA in block pool : write index is larger than pages count" << std::endl;
			std::cin.get();
			return false;
		}
		unsigned int invalid_page_check = 0;
		for (unsigned int i = 0; i < Page_vector_size; i++)
		{
			uint64_t vector = Invalid_page_bitmap[i];
			for (unsigned int j = 0; j < 64; j++)
			{
				if (vector % 2 == 1)
					invalid_page_check++;
				vector = vector / 2;
			}
		}
		if (invalid_page_check != Invalid_page_count)
		{
			std::cout << "INVALID METADATA in block pool : invalid page number wrong " << invalid_page_check << " : " << Invalid_page_count << std::endl;
			std::cin.get();
			return false;
		}
	}

	Block_Pool_Slot_Type* PlaneBookKeepingType::Get_a_free_block(stream_id_type stream_id, bool for_mapping_data)
	{
		Block_Pool_Slot_Type* new_block = NULL;
		new_block = (*Free_block_pool.begin()).second;//Assign a new write frontier block
		if (Free_block_pool.size() == 0)
			PRINT_ERROR("Requesting a free block from an empty pool!")
		Free_block_pool.erase(Free_block_pool.begin());
		new_block->Stream_id = stream_id;
		new_block->Holds_mapping_data = for_mapping_data;
		Block_usage_history.push(new_block->BlockID);

		return new_block;
	}

	Block_Pool_Slot_Type* PlaneBookKeepingType::Get_a_free_slc_block(stream_id_type stream_id, bool for_mapping_data)
	{
		Block_Pool_Slot_Type* new_block = NULL;
		if (Get_free_slc_block_pool_size() == 0)
		{
			//PRINT_ERROR("Requesting a free SLC block from an empty pool!")
			return new_block;
		}
		new_block = Free_slc_block_pool.front();//Assign a new write frontier block
		Free_slc_block_pool.pop();
		if (Free_slc_block_pool.front() == new_block)
		{
			std::cout << "......WRONGWRONGWRONG......" << std::endl;
		}
		new_block->Stream_id = stream_id;
		new_block->Holds_mapping_data = for_mapping_data;
		Slc_Block_usage_history.push(new_block->BlockID);
		if (new_block->Flash_type != Flash_Technology_Type::SLC)
		{
			std::cout << "Get a wrong type of flash block! SLC->TLC" << std::endl;
			std::cin.get();
		}


		return new_block;
	}

	Block_Pool_Slot_Type* PlaneBookKeepingType::Get_a_free_tlc_block(stream_id_type stream_id, bool for_mapping_data)
	{
		Block_Pool_Slot_Type* new_block = NULL;
		if (Free_tlc_block_pool.size() == 0)
		{
			PRINT_ERROR("Requesting a free TLC block from an empty pool!")
			return NULL;
		}
		new_block = Free_tlc_block_pool.front();//Assign a new write frontier block
		Free_tlc_block_pool.pop();
		if (new_block->Flash_type != Flash_Technology_Type::TLC)
		{
			std::cout << "Get a wrong type of flash block! TLC->SLC" << std::endl;
			std::cin.get();
		}
		new_block->Stream_id = stream_id;
		new_block->Holds_mapping_data = for_mapping_data;
		Tlc_Block_usage_history.push(new_block->BlockID);
		return new_block;
	}
	
	void PlaneBookKeepingType::Check_bookkeeping_correctness(const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		if (Total_pages_count != Free_pages_count + Valid_pages_count + Invalid_pages_count)
			PRINT_ERROR("Inconsistent status in the plane bookkeeping record!")
		if (Total_slc_pages_count != Free_slc_pages_count + Valid_slc_pages_count + Invalid_slc_pages_count)
			PRINT_ERROR("Inconsistent status in the plane bookkeeping record SLC pages!" << Free_slc_pages_count << ":" << Valid_slc_pages_count << ":" << Invalid_slc_pages_count)
		if (Total_tlc_pages_count != Free_tlc_pages_count + Valid_tlc_pages_count + Invalid_tlc_pages_count)
			PRINT_ERROR("Inconsistent status in the plane bookkeeping record TLC pages!")
		if (Free_pages_count == 0)
			PRINT_ERROR("Plane " << "@" << plane_address.ChannelID << "@" << plane_address.ChipID << "@" << plane_address.DieID << "@" << plane_address.PlaneID << " pool size: " << Get_free_block_pool_size() << " ran out of free pages! Bad resource management! It is not safe to continue simulation!");
	}

	unsigned int PlaneBookKeepingType::Get_free_block_pool_size()
	{
		return (unsigned int)Free_block_pool.size();
	}

	unsigned int PlaneBookKeepingType::Get_free_slc_block_pool_size()
	{
		return (unsigned int)Free_slc_block_pool.size();
	}

	unsigned int PlaneBookKeepingType::Get_free_tlc_block_pool_size()
	{
		return (unsigned int)Free_tlc_block_pool.size();
	}

	unsigned int PlaneBookKeepingType::Get_free_slc_page_number()
	{
		return (unsigned int)Free_slc_pages_count;
	}

	void PlaneBookKeepingType::Add_to_free_block_pool(Block_Pool_Slot_Type* block, bool consider_dynamic_wl)
	{
		if (consider_dynamic_wl)
		{
			std::pair<unsigned int, Block_Pool_Slot_Type*> entry(block->Erase_count, block);
			Free_block_pool.insert(entry);
		}
		else
		{
			std::pair<unsigned int, Block_Pool_Slot_Type*> entry(0, block);
			Free_block_pool.insert(entry);
		}
	}

	void PlaneBookKeepingType::Add_to_free_slc_block_pool(Block_Pool_Slot_Type* block, bool consider_dynamic_wl)
	{
		//*ZWH*
		//wear leveling is not considerred now
		Free_slc_block_pool.push(block);
		/*
		if (consider_dynamic_wl)
		{
			std::pair<unsigned int, Block_Pool_Slot_Type*> entry(block->Erase_count, block);
			Free_slc_block_pool.insert(entry);
		}
		else
		{
			std::pair<unsigned int, Block_Pool_Slot_Type*> entry(0, block);
			Free_slc_block_pool.insert(entry);
		}
		*/
	}

	void PlaneBookKeepingType::Add_to_free_tlc_block_pool(Block_Pool_Slot_Type* block, bool consider_dynamic_wl)
	{
		Free_tlc_block_pool.push(block);
		/*
		if (consider_dynamic_wl)
		{
			std::pair<unsigned int, Block_Pool_Slot_Type*> entry(block->Erase_count, block);
			Free_tlc_block_pool.insert(entry);
		}
		else
		{
			std::pair<unsigned int, Block_Pool_Slot_Type*> entry(0, block);
			Free_tlc_block_pool.insert(entry);
		}
		*/
	}

	unsigned int Flash_Block_Manager_Base::Get_min_max_erase_difference(const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		unsigned int min_erased_block = 0;
		unsigned int max_erased_block = 0;
		PlaneBookKeepingType *plane_record = &plane_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID][plane_address.PlaneID];

		for (unsigned int i = 1; i < block_no_per_plane; i++)
		{
			if (plane_record->Blocks[i].Erase_count > plane_record->Blocks[i].Erase_count)
				max_erased_block = i;
			if (plane_record->Blocks[i].Erase_count < plane_record->Blocks[i].Erase_count)
				min_erased_block = i;
		}
		return max_erased_block - min_erased_block;
	}


	flash_block_ID_type Flash_Block_Manager_Base::Get_coldest_block_id(const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		unsigned int min_erased_block = 0;
		PlaneBookKeepingType *plane_record = &plane_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID][plane_address.PlaneID];

		for (unsigned int i = 1; i < block_no_per_plane; i++)
		{
			if (plane_record->Blocks[i].Erase_count < plane_record->Blocks[min_erased_block].Erase_count)
				min_erased_block = i;
		}
		return min_erased_block;
	}

	flash_block_ID_type Flash_Block_Manager_Base::Get_coldest_slc_block_id(const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		unsigned int min_erased_block = 65536000;
		PlaneBookKeepingType *plane_record = &plane_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID][plane_address.PlaneID];

		for (unsigned int i = 0; i < slc_block_no_per_plane; i++)
		{
			if (plane_record->Blocks[i].Current_page_write_index < plane_record->Blocks[i].Pages_count)
				continue;
			else if (min_erased_block == 65536000)
				min_erased_block = i;
			else if (plane_record->Blocks[i].Erase_count < plane_record->Blocks[min_erased_block].Erase_count)
				min_erased_block = i;
		}
		if (min_erased_block == 65536000)
		{
			std::cout << "Can't FIND A COLDEST SLC BLOCK." << std::endl;
			std::cin.get();
		}
		return (unsigned int)min_erased_block;
	}

	flash_block_ID_type Flash_Block_Manager_Base::Get_coldest_tlc_block_id(const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		unsigned int min_erased_block = 65536000;
		PlaneBookKeepingType *plane_record = &plane_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID][plane_address.PlaneID];

		for (unsigned int i = slc_block_no_per_plane + 1; i < block_no_per_plane; i++)
		{
			if (plane_record->Blocks[i].Current_page_write_index < plane_record->Blocks[i].Pages_count)
				continue;
			else if (min_erased_block == 65536000)
				min_erased_block = i;
			else if (plane_record->Blocks[i].Erase_count < plane_record->Blocks[min_erased_block].Erase_count)
				min_erased_block = i;
		}
		if (min_erased_block == 65536000)
		{
			std::cout << "Can't FIND A COLDEST TLC BLOCK." << std::endl;
			std::cin.get();
		}
		return min_erased_block;
	}

	PlaneBookKeepingType* Flash_Block_Manager_Base::Get_plane_bookkeeping_entry(const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		return &(plane_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID][plane_address.PlaneID]);
	}

	bool Flash_Block_Manager_Base::Block_has_ongoing_gc_wl(const NVM::FlashMemory::Physical_Page_Address& block_address)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[block_address.ChannelID][block_address.ChipID][block_address.DieID][block_address.PlaneID];
		return plane_record->Blocks[block_address.BlockID].Has_ongoing_gc_wl;
	}
	
	bool Flash_Block_Manager_Base::Can_execute_gc_wl(const NVM::FlashMemory::Physical_Page_Address& block_address)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[block_address.ChannelID][block_address.ChipID][block_address.DieID][block_address.PlaneID];
		return (plane_record->Blocks[block_address.BlockID].Ongoing_user_program_count + plane_record->Blocks[block_address.BlockID].Ongoing_user_read_count == 0);
	}
	
	void Flash_Block_Manager_Base::GC_WL_started(const NVM::FlashMemory::Physical_Page_Address& block_address)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[block_address.ChannelID][block_address.ChipID][block_address.DieID][block_address.PlaneID];
		plane_record->Blocks[block_address.BlockID].Has_ongoing_gc_wl = true;
	}
	
	void Flash_Block_Manager_Base::program_transaction_issued(const NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.PlaneID];
		plane_record->Blocks[page_address.BlockID].Ongoing_user_program_count++;
		if(plane_record->Blocks[page_address.BlockID].Flash_type == Flash_Technology_Type::SLC)
			plane_record->On_going_slc_transaction_num++;
		else if(plane_record->Blocks[page_address.BlockID].Flash_type == Flash_Technology_Type::TLC)
		{
			plane_record->On_going_tlc_transaction_num++;
		}
		else
		{
			std::cout << "Something wrong in calculating the number of program transactions 0" << std::endl;
		}
	}
	
	void Flash_Block_Manager_Base::Read_transaction_issued(const NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.PlaneID];
		plane_record->Blocks[page_address.BlockID].Ongoing_user_read_count++;
		plane_record->Ongoing_user_read_count_plane++;
	}

	void Flash_Block_Manager_Base::Program_transaction_serviced(const NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.PlaneID];
		plane_record->Blocks[page_address.BlockID].Ongoing_user_program_count--;
		if(plane_record->Blocks[page_address.BlockID].Flash_type == Flash_Technology_Type::SLC)
			plane_record->On_going_slc_transaction_num--;
		else if(plane_record->Blocks[page_address.BlockID].Flash_type == Flash_Technology_Type::TLC)
			plane_record->On_going_tlc_transaction_num--;
		else
		{
			std::cout << "Something wrong in calculating the number of program transactions 1" << std::endl;
		}
		if(plane_record->On_going_slc_transaction_num < 0 || plane_record->On_going_tlc_transaction_num < 0)
			std::cout << "Something wrong in calculating the number of program transactions 2" << std::endl;
	}

	void Flash_Block_Manager_Base::Read_transaction_serviced(const NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.PlaneID];
		plane_record->Blocks[page_address.BlockID].Ongoing_user_read_count--;
		plane_record->Ongoing_user_read_count_plane--;
	}
	
	bool Flash_Block_Manager_Base::Is_having_ongoing_program(const NVM::FlashMemory::Physical_Page_Address& block_address)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[block_address.ChannelID][block_address.ChipID][block_address.DieID][block_address.PlaneID];
		return plane_record->Blocks[block_address.BlockID].Ongoing_user_program_count > 0;
	}

	void Flash_Block_Manager_Base::GC_WL_finished(const NVM::FlashMemory::Physical_Page_Address& block_address)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[block_address.ChannelID][block_address.ChipID][block_address.DieID][block_address.PlaneID];
		plane_record->Blocks[block_address.BlockID].Has_ongoing_gc_wl = false;
		if (plane_record->Blocks[block_address.BlockID].Flash_type == Flash_Technology_Type::SLC)
		{
			plane_record->Completed_data_migration++;
			if (plane_record->Completed_data_migration == plane_record->Triggered_data_migration)
			{
				if (plane_record->Data_migration_should_be_terminated == true || plane_record->Triggered_data_migration == Max_data_migration_trigger_one_time)
				{
					plane_record->Triggered_data_migration = 0;
					plane_record->Completed_data_migration = 0;
					plane_record->Doing_data_migration = false;
					plane_record->Data_migration_should_be_terminated = false;
					gc_and_wl_unit->Decrease_plane_num_doing_data_migration();
				}
				else
				{
					gc_and_wl_unit->Check_data_migration_required(plane_record->Get_free_slc_block_pool_size(), block_address);
				}
			}
			else if (plane_record->Triggered_data_migration == Max_data_migration_trigger_one_time)
			{
				plane_record->Data_migration_should_be_terminated = true;
			}
			else if (plane_record->Data_migration_should_be_terminated == false)
			{
				//gc_and_wl_unit->Check_data_migration_required(plane_record->Get_free_slc_block_pool_size(), block_address);
			}
		}
		else
		{
			plane_record->Completed_garbage_collection++;
			if (plane_record->Completed_garbage_collection == plane_record->Triggered_garbage_collection)
			{
				plane_record->Triggered_garbage_collection = 0;
				plane_record->Completed_garbage_collection = 0;
				plane_record->Doing_garbage_collection = false;
			}
			gc_and_wl_unit->Check_tlc_gc_required(plane_record->Get_free_tlc_block_pool_size(), block_address);
		}
		
	}
	
	bool Flash_Block_Manager_Base::Is_page_valid(Block_Pool_Slot_Type* block, flash_page_ID_type page_id)
	{
		if ((block->Invalid_page_bitmap[page_id / 64] & (((uint64_t)1) << page_id)) == 0)
			return true;
		return false;
	}

	bool Flash_Block_Manager_Base::Is_data_migration_needed()
	{
		for (flash_channel_ID_type chann_id = 0; chann_id < channel_count; chann_id++)
		{
			for (flash_chip_ID_type chip_id = 0; chip_id < chip_no_per_channel; chip_id++)
			{
				for (flash_die_ID_type die_id = 0; die_id < die_no_per_chip; die_id++)
				{
					for (flash_plane_ID_type plane_id = 0; plane_id < plane_no_per_die; plane_id++)
					{
						PlaneBookKeepingType* pbke = &plane_manager[chann_id][chip_id][die_id][plane_id];
						if (pbke->Get_free_slc_block_pool_size() < slc_block_no_per_plane * 0.8)
							return true;
					}
				}
			}
		}
		return false;
	}
}