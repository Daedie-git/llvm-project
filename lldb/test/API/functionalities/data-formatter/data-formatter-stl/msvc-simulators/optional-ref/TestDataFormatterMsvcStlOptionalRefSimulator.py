"""Simulate std::optional<T&> pointer layout and check the formatter."""

from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil


class OptionalRefSimulatorTestCase(TestBase):
    NO_DEBUG_INFO_TESTCASE = True

    def test(self):
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.cpp")
        )

        engaged = self.frame().FindVariable("engaged")
        self.assertTrue(engaged.IsValid())
        self.assertEqual(engaged.GetNumChildren(), 1)
        self.assertEqual(engaged.GetChildAtIndex(0).GetName(), "Value")
        self.assertEqual(engaged.GetChildAtIndex(0).GetValueAsSigned(), 42)
        self.expect("frame variable engaged", substrs=["Has Value=true", "Value = 42"])

        empty = self.frame().FindVariable("empty")
        self.assertTrue(empty.IsValid())
        self.assertEqual(empty.GetNumChildren(), 0)
        self.expect("frame variable empty", substrs=["Has Value=false"])

        engaged_s = self.frame().FindVariable("engaged_s")
        self.assertEqual(engaged_s.GetNumChildren(), 1)
        self.assertEqual(
            engaged_s.GetChildAtIndex(0).GetChildMemberWithName("a").GetValueAsSigned(),
            7,
        )
        self.expect("frame variable engaged_s", substrs=["Has Value=true", "a = 7"])

        empty_s = self.frame().FindVariable("empty_s")
        self.assertEqual(empty_s.GetNumChildren(), 0)
        self.expect("frame variable empty_s", substrs=["Has Value=false"])
