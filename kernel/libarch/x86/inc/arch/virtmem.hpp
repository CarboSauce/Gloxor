#pragma once
#include "gloxor/types.hpp"
namespace arch
{
	constexpr auto pageSize = 0x1000;
	/**
    * @brief class for managing virtual memory context 
    * 
    */
    struct virtCtxImpl;
	struct virtContext
	{
      virtCtxImpl* context; 
	  public:
        virtContext() = default;
        virtContext(virtCtxImpl* ctx) : context{ctx} {};
		/**
       * @brief Map virtual address to physical address
       * 
       * @param from Virtual address to map from
       * @param to Physical address to map to
       * @return true Success
       * @return false Mapping failed
       */
		bool map(const void* from,const void* to);
		/**
       * @brief Unmap virtual address from current context
       * 
       * @param whichVirtual which virtual address to unamp
       * @return true Success
       * @return false Unmapping failed, if even possible 
       */
		bool unmap(const void* whichVirtual);
		/**
       * @brief Switch current context to one provided
       * 
       */
		void setContext();
		/**
       * @brief Translate virtual address to physical address
       * 
       * @param from Virtual address to translate
       * @return void* Physical Address from translation
       */
		void* translate(const void* from);
		/**
       * @todo 
       *  Generally virtual memory mapping shouldn't be copied 
       *  unless we wanna implement some sort of fork() syscall
       *  revisit that incase this becomes dumb choice
       */
		virtContext& operator=(const virtContext&) = delete;
		virtContext(const virtContext&) = delete;
		virtContext(virtContext&&) = default;
		virtContext& operator=(virtContext&&) = default;
	};

	/**
    * @brief Initialize Virtual memory context
    * @note Possibly allocating, hence we need to return error
    * @note I'd like it to be a constructor, but we dont have exception handling to propagate error
    * @return true Success
    * @return false Allocation failure 
    */
    bool virtInitContext(virtContext*);
    void virtFlush(void* addr);
    void virtCacheFlush();
} // namespace arch