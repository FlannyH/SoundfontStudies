#include "pch.h"
#include "CppUnitTest.h"
#include "../SoundfontStudies/soundfont.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTest
{
	TEST_CLASS(FixedPoint)
	{
	public:
		
		TEST_METHOD(SetDouble)
		{
			fixed_32_32 fixed1 = 1.0;
			fixed_32_32 fixed2 = 3.14159265359;
			fixed_32_32 fixed3 = 1.5;
			Assert::AreEqual(fixed1.integer, (i32)1);
			Assert::AreEqual(fixed1.fraction, (u32)0);
			Assert::AreEqual(fixed2.integer, (i32)3);
			Assert::AreEqual(fixed2.fraction, (u32)0x243f6a88);
			Assert::AreEqual(fixed3.integer, (i32)1);
			Assert::AreEqual(fixed3.fraction, (u32)0x80000000);
			Assert::AreNotEqual(fixed2.fraction, (u32)0x12345678);
		}
		TEST_METHOD(Multiply)
		{
			fixed_32_32 fixed1 = 1.5;
			fixed_32_32 fixed2 = 2.5;
			fixed_32_32 fixed3 = fixed1 * fixed2;
			fixed_32_32 fixed4 = 1.5 * 2.5;
			Assert::AreEqual(fixed3.integer, fixed4.integer);
			Assert::AreEqual(fixed3.fraction, fixed4.fraction);
		}
	};
}
