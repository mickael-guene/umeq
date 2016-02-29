add_subdirectory (loader32)
# when host is 32 bits then we don't support 64 bits guest. So avoid building
# useless loader64
if (UMEQ_ARCH_HOST_SIZE EQUAL 64)
	add_subdirectory (loader64)
endif (UMEQ_ARCH_HOST_SIZE EQUAL 64)