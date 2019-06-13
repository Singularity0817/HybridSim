#ifndef BLOCK_POOL_MANAGER_BASE_H
#define BLOCK_POOL_MANAGER_BASE_H

#include <list>
#include <cstdint>
#include <queue>
#include <set>
#include "../nvm_chip/flash_memory/FlashTypes.h"
#include "../nvm_chip/flash_memory/Physical_Page_Address.h"
#include "GC_and_WL_Unit_Base.h"
#include "../nvm_chip/flash_memory/FlashTypes.h"

namespace SSD_Components
{
#define All_VALID_PAGE 0x0000000000000000ULL
	class GC_and_WL_Unit_Base;
	/*
	* Block_Service_Status is used to impelement a state machine for each physical block in order to
	* eliminate race conditions between GC page movements and normal user I/O requests.
	* Allowed transitions:
	* 1: IDLE -> GC, IDLE -> USER
	* 2: GC -> IDLE, GC -> GC_UWAIT
	* 3: USER -> IDLE, USER -> GC_USER
	* 4: GC_UWAIT -> GC, GC_UWAIT -> GC_UWAIT
	* 5: GC_USER -> GC
	*/
	enum class Block_Service_Status {IDLE, GC_WL, USER, GC_USER, GC_UWAIT, GC_USER_UWAIT};
	class Block_Pool_Slot_Type
	{
	public:
		flash_block_ID_type BlockID;
		//*ZWH*
		Flash_Technology_Type Flash_type;
		unsigned int Pages_count;
		//*ZWH*
		flash_page_ID_type Current_page_write_index;
		Block_Service_Status Current_status;
		unsigned int Invalid_page_count;
		unsigned int Erase_count;
		static unsigned int Page_vector_size;
		uint64_t* Invalid_page_bitmap;//A bit sequence that keeps track of valid/invalid status of pages in the block. A "0" means valid, and a "1" means invalid.
		stream_id_type Stream_id = NO_STREAM;
		bool Holds_mapping_data = false;
		bool Has_ongoing_gc_wl = false;
		NVM_Transaction_Flash_ER* Erase_transaction;
		bool Hot_block = false;//Used for hot/cold separation mentioned in the "On the necessity of hot and cold data identification to reduce the write amplification in flash-based SSDs", Perf. Eval., 2014.
		int Ongoing_user_read_count;
		int Ongoing_user_program_count;
		void Erase();
		bool Check_Block_Metadata();
	};

	class PlaneBookKeepingType
	{
	public:
		unsigned int Total_pages_count;
		unsigned int Free_pages_count;
		unsigned int Valid_pages_count;
		unsigned int Invalid_pages_count;
		//*ZWH*
		unsigned int Total_slc_pages_count;
		unsigned int Free_slc_pages_count;
		unsigned int Valid_slc_pages_count;
		unsigned int Invalid_slc_pages_count;
		unsigned int Total_tlc_pages_count;
		unsigned int Free_tlc_pages_count;
		unsigned int Valid_tlc_pages_count;
		unsigned int Invalid_tlc_pages_count;
		//*ZWH*
		Block_Pool_Slot_Type* Blocks;
		std::multimap<unsigned int, Block_Pool_Slot_Type*> Free_block_pool;
		Block_Pool_Slot_Type** Data_wf, ** GC_wf; //The write frontier blocks for data and GC pages. MQSim adopts Double Write Frontier approach for user and GC writes which is shown very advantages in: B. Van Houdt, "On the necessity of hot and cold data identification to reduce the write amplification in flash - based SSDs", Perf. Eval., 2014
		Block_Pool_Slot_Type** Translation_wf; //The write frontier blocks for translation GC pages
		std::queue<flash_block_ID_type> Block_usage_history;//A fifo queue that keeps track of flash blocks based on their usage history
		std::queue<flash_block_ID_type> Slc_Block_usage_history;
		std::queue<flash_block_ID_type> Tlc_Block_usage_history;
		std::set<flash_block_ID_type> Ongoing_erase_operations;
		std::set<flash_block_ID_type> Ongoing_slc_erase_operations;
		//*ZWH*
		Block_Pool_Slot_Type* Slc_blocks;
		Block_Pool_Slot_Type* Tlc_blocks;
		//std::multimap<unsigned int, Block_Pool_Slot_Type*> Free_slc_block_pool;
		//std::multimap<unsigned int, Block_Pool_Slot_Type*> Free_tlc_block_pool;
		std::queue<Block_Pool_Slot_Type *> Free_slc_block_pool;
		
		Block_Pool_Slot_Type** Data_slc_wf, ** Data_tlc_wf;
		std::queue<Block_Pool_Slot_Type *> Free_tlc_block_pool;
		//*ZWH*
		Block_Pool_Slot_Type* Get_a_free_block(stream_id_type stream_id, bool for_mapping_data);
		//*ZWH*
		Block_Pool_Slot_Type* Get_a_free_slc_block(stream_id_type stream_id, bool for_mapping_data);
		Block_Pool_Slot_Type* Get_a_free_tlc_block(stream_id_type stream_id, bool for_mapping_data);
		//*ZWH*
		unsigned int Get_free_block_pool_size();
		//*ZWH*
		unsigned int Get_free_slc_block_pool_size();
		unsigned int Get_free_tlc_block_pool_size();
		unsigned int Get_free_slc_page_number();
		//*ZWH*
		void Check_bookkeeping_correctness(const NVM::FlashMemory::Physical_Page_Address& plane_address);
		void Add_to_free_block_pool(Block_Pool_Slot_Type* block, bool consider_dynamic_wl);
		//*ZWH*
		void Add_to_free_slc_block_pool(Block_Pool_Slot_Type* block, bool consider_dynamic_wl);
		void Add_to_free_tlc_block_pool(Block_Pool_Slot_Type* block, bool consider_dynamic_wl);
		//*ZWH*
		int On_going_slc_transaction_num;
		int On_going_tlc_transaction_num;
		int Triggered_data_migration;
		int Completed_data_migration;
		bool Doing_data_migration;
		bool Data_migration_should_be_terminated;
		bool Doing_garbage_collection = false;
		int Triggered_garbage_collection = 0;
		int Completed_garbage_collection = 0;
		int Ongoing_user_read_count_plane = 0;
	};

	class Flash_Block_Manager_Base
	{
		friend class Address_Mapping_Unit_Page_Level;
		friend class GC_and_WL_Unit_Page_Level;
		friend class GC_and_WL_Unit_Base;
	public:
		Flash_Block_Manager_Base(GC_and_WL_Unit_Base* gc_and_wl_unit, unsigned int max_allowed_block_erase_count, unsigned int total_concurrent_streams_no,
			unsigned int channel_count, unsigned int chip_no_per_channel, unsigned int die_no_per_chip, unsigned int plane_no_per_die,
			unsigned int block_no_per_plane, unsigned int page_no_per_block);
		Flash_Block_Manager_Base(GC_and_WL_Unit_Base* gc_and_wl_unit, unsigned int max_allowed_block_erase_count, unsigned int total_concurrent_streams_no,
			unsigned int channel_count, unsigned int chip_no_per_channel, unsigned int die_no_per_chip, unsigned int plane_no_per_die,
			unsigned int block_no_per_plane, unsigned int page_no_per_block, unsigned int slc_block_no_per_plane, unsigned int page_no_per_slc_block);
		virtual ~Flash_Block_Manager_Base();
		virtual void Allocate_block_and_page_in_plane_for_user_write(const stream_id_type streamID, NVM::FlashMemory::Physical_Page_Address& address) = 0;
		//*ZWH*
		virtual void Allocate_block_and_page_in_plane_for_user_write(const stream_id_type streamID, NVM::FlashMemory::Physical_Page_Address& address, Flash_Technology_Type flash_type) = 0;
		virtual bool Allocate_slc_block_and_page_in_plane_for_user_write(const stream_id_type stream_id, NVM::FlashMemory::Physical_Page_Address& address) = 0;
		virtual void Allocate_tlc_block_and_page_in_plane_for_user_write(const stream_id_type stream_id, NVM::FlashMemory::Physical_Page_Address& address) = 0;
		//*ZWH*
		virtual void Allocate_block_and_page_in_plane_for_gc_write(const stream_id_type streamID, NVM::FlashMemory::Physical_Page_Address& address) = 0;
		virtual void Allocate_block_and_page_in_plane_for_translation_write(const stream_id_type streamID, NVM::FlashMemory::Physical_Page_Address& address, bool is_for_gc) = 0;
		virtual void Allocate_Pages_in_block_and_invalidate_remaining_for_preconditioning(const stream_id_type stream_id, const NVM::FlashMemory::Physical_Page_Address& plane_address, std::vector<NVM::FlashMemory::Physical_Page_Address>& page_addresses) = 0;
		virtual void Invalidate_page_in_block(const stream_id_type streamID, const NVM::FlashMemory::Physical_Page_Address& address) = 0;
		virtual void Invalidate_page_in_block_for_preconditioning(const stream_id_type streamID, const NVM::FlashMemory::Physical_Page_Address& address) = 0;
		virtual void Add_erased_block_to_pool(const NVM::FlashMemory::Physical_Page_Address& address) = 0;
		virtual unsigned int Get_pool_size(const NVM::FlashMemory::Physical_Page_Address& plane_address) = 0;
		virtual unsigned int Get_slc_pool_size(const NVM::FlashMemory::Physical_Page_Address& plane_address) = 0;
		virtual unsigned int Get_tlc_pool_size(const NVM::FlashMemory::Physical_Page_Address& plane_address) = 0;
		flash_block_ID_type Get_coldest_block_id(const NVM::FlashMemory::Physical_Page_Address& plane_address);
		//*ZWH*
		flash_block_ID_type Get_coldest_slc_block_id(const NVM::FlashMemory::Physical_Page_Address& plane_address);
		flash_block_ID_type Get_coldest_tlc_block_id(const NVM::FlashMemory::Physical_Page_Address& plane_address);
		//*ZWH*
		unsigned int Get_min_max_erase_difference(const NVM::FlashMemory::Physical_Page_Address& plane_address);
		void Set_GC_and_WL_Unit(GC_and_WL_Unit_Base* );
		PlaneBookKeepingType* Get_plane_bookkeeping_entry(const NVM::FlashMemory::Physical_Page_Address& plane_address);
		bool Block_has_ongoing_gc_wl(const NVM::FlashMemory::Physical_Page_Address& block_address);//Checks if there is an ongoing gc for block_address
		bool Can_execute_gc_wl(const NVM::FlashMemory::Physical_Page_Address& block_address);//Checks if the gc request can be executed on block_address (there shouldn't be any ongoing user read/program requests targeting block_address)
		void GC_WL_started(const NVM::FlashMemory::Physical_Page_Address& block_address);//Updates the block bookkeeping record
		void GC_WL_finished(const NVM::FlashMemory::Physical_Page_Address& block_address);//Updates the block bookkeeping record
		void Read_transaction_issued(const NVM::FlashMemory::Physical_Page_Address& page_address);//Updates the block bookkeeping record
		void Read_transaction_serviced(const NVM::FlashMemory::Physical_Page_Address& page_address);//Updates the block bookkeeping record
		void Program_transaction_serviced(const NVM::FlashMemory::Physical_Page_Address& page_address);//Updates the block bookkeeping record
		bool Is_having_ongoing_program(const NVM::FlashMemory::Physical_Page_Address& block_address);//Cheks if block has any ongoing program request
		bool Is_page_valid(Block_Pool_Slot_Type* block, flash_page_ID_type page_id);//Make the page invalid in the block bookkeeping record
		int Max_data_migration_trigger_one_time;
		bool Is_data_migration_needed();
	protected:
		PlaneBookKeepingType ****plane_manager;//Keeps track of plane block usage information
		GC_and_WL_Unit_Base *gc_and_wl_unit;
		unsigned int max_allowed_block_erase_count;
		unsigned int total_concurrent_streams_no;
		unsigned int channel_count;
		unsigned int chip_no_per_channel;
		unsigned int die_no_per_chip;
		unsigned int plane_no_per_die;
		unsigned int block_no_per_plane;
		unsigned int pages_no_per_block;
		//*ZWH*
		unsigned int slc_block_no_per_plane;
		unsigned int page_no_per_slc_block;
		unsigned int tlc_block_no_per_plane;
		unsigned int page_no_per_tlc_block;
		//*ZWH*
		void program_transaction_issued(const NVM::FlashMemory::Physical_Page_Address& page_address);//Updates the block bookkeeping record
	};
}

#endif//!BLOCK_POOL_MANAGER_BASE_H
