#include "lib/self_test.h"

#include "lib/lib.h"

class TestLib : public CxxTest::TestSuite 
{
public:
	// 16-bit saturating arithmetic
	void test_addusw()
	{
		TS_ASSERT_EQUALS(addusw(4u, 0x100u), 0x0104u);
		TS_ASSERT_EQUALS(addusw(0u, 0xFFFFu), 0xFFFFu);
		TS_ASSERT_EQUALS(addusw(0x8000u, 0x8000u), 0xFFFFu);
		TS_ASSERT_EQUALS(addusw(0xFFF0u, 0x0004u), 0xFFF4u);
		TS_ASSERT_EQUALS(addusw(0xFFFFu, 0xFFFFu), 0xFFFFu);
	}

	void test_subusw()
	{
		TS_ASSERT_EQUALS(subusw(4u, 0x100u), 0u);
		TS_ASSERT_EQUALS(subusw(100u, 90u), 10u);
		TS_ASSERT_EQUALS(subusw(0x8000u, 0x8000u), 0u);
		TS_ASSERT_EQUALS(subusw(0x0FFFu, 0xFFFFu), 0u);
		TS_ASSERT_EQUALS(subusw(0xFFFFu, 0x0FFFu), 0xF000u);
	}

	void test_hi_lo()
	{
		TS_ASSERT_EQUALS(u64_hi(0x0123456789ABCDEFull), 0x01234567u);
		TS_ASSERT_EQUALS(u64_hi(0x0000000100000002ull), 0x00000001u);

		TS_ASSERT_EQUALS(u64_lo(0x0123456789ABCDEFull), 0x89ABCDEFu);
		TS_ASSERT_EQUALS(u64_lo(0x0000000100000002ull), 0x00000002u);

		TS_ASSERT_EQUALS(u32_hi(0x01234567u), 0x0123u);
		TS_ASSERT_EQUALS(u32_hi(0x00000001u), 0x0000u);

		TS_ASSERT_EQUALS(u32_lo(0x01234567u), 0x4567u);
		TS_ASSERT_EQUALS(u32_lo(0x00000001u), 0x0001u);

		TS_ASSERT_EQUALS(u64_from_u32(0xFFFFFFFFu, 0x80000008u), 0xFFFFFFFF80000008ull);
		TS_ASSERT_EQUALS(u32_from_u16(0x8000u, 0xFFFFu), 0x8000FFFFu);
	}

	// fp_to_u?? already validate the result.
};
