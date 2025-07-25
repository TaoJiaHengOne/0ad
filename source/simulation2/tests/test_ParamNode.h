/* Copyright (C) 2024 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "lib/self_test.h"

#include "simulation2/system/ParamNode.h"

#include "ps/CLogger.h"
#include "ps/XML/Xeromyces.h"

#include <optional>

class TestParamNode : public CxxTest::TestSuite
{
	std::optional<CXeromycesEngine> xeromycesEngine;
public:
	void setUp()
	{
		xeromycesEngine.emplace();
	}

	void tearDown()
	{
		xeromycesEngine.reset();
	}

	void test_basic()
	{
		CParamNode node;
		TS_ASSERT_EQUALS(CParamNode::LoadXMLString(node, "<test> <Foo> 1 </Foo><Bar>2<Baz>3</Baz>4</Bar><Qux/></test>"), PSRETURN_OK);
		TS_ASSERT(node.GetChild("test").IsOk());
		TS_ASSERT(!node.GetChild("Test").IsOk());
		TS_ASSERT_STR_EQUALS(node.GetChild("test").ToString(), "");
		TS_ASSERT(node.GetChild("test").GetChild("Foo").IsOk());
		TS_ASSERT_EQUALS(node.GetChild("test").GetChild("Foo").ToInt(), 1);
		TS_ASSERT_STR_EQUALS(node.GetChild("test").GetChild("Foo").ToString(), "1");
		TS_ASSERT_EQUALS(node.GetChild("test").GetChild("Bar").ToInt(), 24);
		TS_ASSERT_STR_EQUALS(node.GetChild("test").GetChild("Bar").ToString(), "24");
		TS_ASSERT_EQUALS(node.GetChild("test").GetChild("Bar").GetChild("Baz").ToInt(), 3);
		TS_ASSERT(node.GetChild("test").GetChild("Qux").IsOk());
		TS_ASSERT(!node.GetChild("test").GetChild("Qux").GetChild("Baz").IsOk());
		TS_ASSERT_STR_EQUALS(node.GetChild("test").ToXMLString(), node.GetOnlyChild().ToXMLString());

		CParamNode nullOne(false);
		CParamNode nullTwo = nullOne;
		CParamNode nullThree(nullOne);
		TS_ASSERT(!nullOne.IsOk());
		TS_ASSERT(!nullTwo.IsOk());
		TS_ASSERT(!nullThree.IsOk());

		TS_ASSERT_STR_EQUALS(nullOne.ToString(), "");
		TS_ASSERT(nullOne.ToInt() == 0);
		TS_ASSERT(nullOne.ToFixed().ToDouble() == 0);
	}

	void test_attrs()
	{
		CParamNode node;
		TS_ASSERT_EQUALS(CParamNode::LoadXMLString(node, "<test x='1' y='2'> <z>3</z> <w a='4'/></test>"), PSRETURN_OK);
		TS_ASSERT(node.GetChild("test").IsOk());
		TS_ASSERT(node.GetChild("test").GetChild("@x").IsOk());
		TS_ASSERT(node.GetChild("test").GetChild("@y").IsOk());
		TS_ASSERT(node.GetChild("test").GetChild("z").IsOk());
		TS_ASSERT(node.GetChild("test").GetChild("w").IsOk());
		TS_ASSERT(node.GetChild("test").GetChild("w").GetChild("@a").IsOk());
		TS_ASSERT_EQUALS(node.GetChild("test").GetChild("@x").ToInt(), 1);
		TS_ASSERT_EQUALS(node.GetChild("test").GetChild("@y").ToInt(), 2);
		TS_ASSERT_EQUALS(node.GetChild("test").GetChild("z").ToInt(), 3);
		TS_ASSERT_EQUALS(node.GetChild("test").GetChild("w").GetChild("@a").ToInt(), 4);
	}

	void test_ToXMLString()
	{
		CParamNode node;
		TS_ASSERT_EQUALS(CParamNode::LoadXMLString(node, "<test x='1' y='2'> <z>3</z> <w a='4'/></test>"), PSRETURN_OK);
		TS_ASSERT_STR_EQUALS(node.ToXMLString(), "<test x=\"1\" y=\"2\"><w a=\"4\"></w><z>3</z></test>");
	}

	void test_overlay_basic()
	{
		CParamNode node;
		TS_ASSERT_EQUALS(CParamNode::LoadXMLString(node, "<test x='1' y='2'> <a>3</a> <b>4</b> </test>"), PSRETURN_OK);
		TS_ASSERT_EQUALS(CParamNode::LoadXMLString(node, "<test y='5' z='6'> <b>7</b> <c>8</c> </test>"), PSRETURN_OK);
		TS_ASSERT_STR_EQUALS(node.ToXMLString(), "<test x=\"1\" y=\"5\" z=\"6\"><a>3</a><b>7</b><c>8</c></test>");
	}

	void test_overlay_disable()
	{
		CParamNode node;
		TS_ASSERT_EQUALS(CParamNode::LoadXMLString(node, "<test> <a>1</a> <b>2</b> </test>"), PSRETURN_OK);
		TS_ASSERT_EQUALS(CParamNode::LoadXMLString(node, "<test> <a disable=''/> <c disable=''/> </test>"), PSRETURN_OK);
		TS_ASSERT_STR_EQUALS(node.ToXMLString(), "<test><b>2</b></test>");
	}

	void test_overlay_replace()
	{
		CParamNode node;
		TS_ASSERT_EQUALS(CParamNode::LoadXMLString(node, "<test> <a x='1'>2<b/></a> <c y='3'/></test>"), PSRETURN_OK);
		TS_ASSERT_EQUALS(CParamNode::LoadXMLString(node, "<test> <a replace=''><d/></a> <e replace=''/> </test>"), PSRETURN_OK);
		TS_ASSERT_STR_EQUALS(node.ToXMLString(), "<test><a><d></d></a><c y=\"3\"></c><e></e></test>");
	}

	void test_overlay_tokens()
	{
		CParamNode node;
		TS_ASSERT_EQUALS(CParamNode::LoadXMLString(node, "<test> <a datatype='tokens'>x y</a><b datatype='tokens'>a  b\nc\td</b><c datatype='tokens'>m n</c></test>"), PSRETURN_OK);
		TS_ASSERT_EQUALS(CParamNode::LoadXMLString(node, "<test> <a datatype='tokens'>-y z w</a><c datatype='tokens' replace=''>n   o</c></test>"), PSRETURN_OK);
		TS_ASSERT_STR_EQUALS(node.ToXMLString(), "<test><a datatype=\"tokens\">x z w</a><b datatype=\"tokens\">a b c d</b><c datatype=\"tokens\">n o</c></test>");
	}

	void test_overlay_remove_nonexistent_token()
	{
		// regression test; this used to cause a crash because of a failure to check whether the token being removed was present
		TestLogger logger;
		CParamNode node;
		TS_ASSERT_EQUALS(CParamNode::LoadXMLString(node, "<test> <a datatype='tokens'>-nonexistenttoken X</a></test>"), PSRETURN_OK);
		TS_ASSERT_STR_EQUALS(node.ToXMLString(), "<test><a datatype=\"tokens\">X</a></test>");
	}

	void test_overlay_remove_empty_token()
	{
		TestLogger logger;
		CParamNode node;
		TS_ASSERT_EQUALS(CParamNode::LoadXMLString(node, "<test> <a datatype='tokens'>  Y  -  X </a></test>"), PSRETURN_OK);
		TS_ASSERT_STR_EQUALS(node.ToXMLString(), "<test><a datatype=\"tokens\">Y X</a></test>");
	}

	void test_overlay_filtered()
	{
		CParamNode node;
		TS_ASSERT_EQUALS(CParamNode::LoadXMLString(node, "<test> <a><b/></a> <c>toberemoved</c> <d><e/></d> </test>"), PSRETURN_OK);
		TS_ASSERT_EQUALS(CParamNode::LoadXMLString(node, "<test filtered=\"\"> <a/> <d><f/></d> <g/> </test>"), PSRETURN_OK);
		TS_ASSERT_STR_EQUALS(node.ToXMLString(), "<test><a><b></b></a><d><e></e><f></f></d><g></g></test>");

		CParamNode node2;
		TS_ASSERT_EQUALS(CParamNode::LoadXMLString(node2, "<test> <a><b>b</b><c>c</c><d>d</d><e>e</e></a> <f/> </test>"), PSRETURN_OK);
		TS_ASSERT_EQUALS(CParamNode::LoadXMLString(node2, "<test filtered=\"\"> <a filtered=\"\"><b merge=\"\"/><c>c2</c><d/></a> </test>"), PSRETURN_OK);
		TS_ASSERT_STR_EQUALS(node2.ToXMLString(), "<test><a><b>b</b><c>c2</c><d></d></a></test>");
	}

	void test_overlay_merge()
	{
		CParamNode node;
		TS_ASSERT_EQUALS(CParamNode::LoadXMLString(node, "<test> <a><b>foo</b><c>bar</c></a> <x><y><z>foo</z></y></x> </test>"), PSRETURN_OK);
		TS_ASSERT_EQUALS(CParamNode::LoadXMLString(node, "<test> <a merge=\"\"><b>test</b><d>baz</d></a> <i merge=\"\"><j>willnotbeincluded</j></i> <x merge=\"\"><y merge=\"\"><v>text</v></y><w>more text</w></x> </test>"), PSRETURN_OK);
		TS_ASSERT_STR_EQUALS(node.ToXMLString(), "<test><a><b>test</b><c>bar</c><d>baz</d></a><x><w>more text</w><y><v>text</v><z>foo</z></y></x></test>");
	}

	void test_overlay_merge_empty()
	{
		// 'merge' nodes don't change the original value.
		CParamNode node;
		TS_ASSERT_EQUALS(CParamNode::LoadXMLString(node, "<test><a>foo</a><c>bar</c></test>"), PSRETURN_OK);
		TS_ASSERT_EQUALS(CParamNode::LoadXMLString(node, "<test><a merge=\"\"/><b merge=\"\">skipped</b><c merge=\"\">replaced</c></test>"), PSRETURN_OK);
		TS_ASSERT_STR_EQUALS(node.ToXMLString(), "<test><a>foo</a><c>bar</c></test>");
	}

	void test_overlay_filtered_merge()
	{
		CParamNode node;
		TS_ASSERT_EQUALS(CParamNode::LoadXMLString(node, "<test> <a><b/></a> <c><x/></c> <Health><Max>1200</Max></Health> </test>"), PSRETURN_OK);
		TS_ASSERT_EQUALS(CParamNode::LoadXMLString(node, "<test filtered=\"\"> <c merge=\"\"/> <d>bar</d> <e merge=\"\"/> <Health><Initial>1</Initial></Health> </test>"), PSRETURN_OK);
		TS_ASSERT_STR_EQUALS(node.ToXMLString(), "<test><Health><Initial>1</Initial><Max>1200</Max></Health><c><x></x></c><d>bar</d></test>");
	}

	void test_overlay_ops()
	{
		CParamNode node;
		TS_ASSERT_EQUALS(CParamNode::LoadXMLString(node, "<test><a>5</a><b>5</b><c>5</c></test>"), PSRETURN_OK);
		TS_ASSERT_EQUALS(CParamNode::LoadXMLString(node, "<test><a op='add'>5</a><b op='mul'>3</b><c op='mul_round'>0.55</c></test>"), PSRETURN_OK);
		TS_ASSERT_STR_EQUALS(node.ToXMLString(), "<test><a>10</a><b>15</b><c>3</c></test>");
	}

	void test_types()
	{
		CParamNode node;
		TS_ASSERT_EQUALS(CParamNode::LoadXMLString(node, "<test><n>+010.75</n><t>true</t></test>"), PSRETURN_OK);
		TS_ASSERT(node.GetChild("test").IsOk());
		TS_ASSERT(node.GetChild("test").GetChild("n").IsOk());
		TS_ASSERT_EQUALS(node.GetChild("test").GetChild("n").ToString(), "+010.75");
		TS_ASSERT_EQUALS(node.GetChild("test").GetChild("n").ToInt(), 10);
		TS_ASSERT_EQUALS(node.GetChild("test").GetChild("n").ToFixed().ToDouble(), 10.75);
		TS_ASSERT_EQUALS(node.GetChild("test").GetChild("n").ToBool(), false);
		TS_ASSERT_EQUALS(node.GetChild("test").GetChild("t").ToBool(), true);
	}

	void test_escape()
	{
		TS_ASSERT_STR_EQUALS(CParamNode::EscapeXMLString("test"), "test");
		TS_ASSERT_STR_EQUALS(CParamNode::EscapeXMLString("x < y << z"), "x &lt; y &lt;&lt; z");
		TS_ASSERT_STR_EQUALS(CParamNode::EscapeXMLString("x < y \"&' y > z ]]> "), "x &lt; y &quot;&amp;' y &gt; z ]]&gt; ");
		TS_ASSERT_STR_EQUALS(CParamNode::EscapeXMLString(" \r\n\t "), " &#13;&#10;&#9; ");
	}
};
