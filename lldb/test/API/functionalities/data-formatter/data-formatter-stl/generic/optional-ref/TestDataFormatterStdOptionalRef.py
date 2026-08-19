"""Test std::optional<T&> summaries and synthetic children."""

import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil


class StdOptionalRefTestCase(TestBase):
    SHARED_BUILD_TESTCASE = False
    TEST_WITH_PDB_DEBUG_INFO = True

    def do_test(self):
        lldbutil.run_to_source_breakpoint(
            self, "// break here", lldb.SBFileSpec("main.cpp")
        )

        has_optional_ref = self.frame().FindVariable("has_optional_ref")
        self.assertTrue(has_optional_ref.IsValid())
        if has_optional_ref.GetValueAsUnsigned() == 0:
            self.skipTest("std::optional<T&> is not available")

        engaged = self.frame().FindVariable("engaged")
        self.assertTrue(engaged.GetError().Success())
        self.assertEqual(engaged.GetNumChildren(), 1)
        self.assertEqual(engaged.GetChildAtIndex(0).GetValueAsSigned(), 42)
        self.expect("frame variable engaged", substrs=["Has Value=true", "Value = 42"])
        self.expect_var_path("*engaged", value="42")

        empty = self.frame().FindVariable("empty")
        self.assertTrue(empty.GetError().Success())
        self.assertEqual(empty.GetNumChildren(), 0)
        self.expect("frame variable empty", substrs=["Has Value=false"])

        engaged_s = self.frame().FindVariable("engaged_s")
        self.assertTrue(engaged_s.GetError().Success())
        self.assertEqual(engaged_s.GetNumChildren(), 1)
        self.expect("frame variable engaged_s", substrs=["Has Value=true", "hello"])

        empty_s = self.frame().FindVariable("empty_s")
        self.assertEqual(empty_s.GetNumChildren(), 0)
        self.expect("frame variable empty_s", substrs=["Has Value=false"])

    @add_test_categories(["libstdcxx"])
    def test_libstdcxx(self):
        self.build(dictionary={"USE_LIBSTDCPP": 1})
        self.do_test()

    @add_test_categories(["libc++"])
    def test_libcxx(self):
        self.build(dictionary={"USE_LIBCPP": 1})
        self.do_test()

    @add_test_categories(["msvcstl"])
    def test_msvcstl(self):
        self.build()
        self.do_test()
