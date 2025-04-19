#pragma once

#include "Utility.h"
#include <fmt/core.h>
#include <span>

namespace Raysharp {

 /* The maximum number of reported coordinate points */
 constexpr int IVA_REPORT_COORD_NUM = 16;

#pragma pack(1)
enum tagMetaDataType
{
    META_DATA_TYPE_RESULT_V2 = 0x10, /* IPC Intelligent analysis result data */
    META_DATA_TYPE_RULE_V2 = 0x11, /* IPC Intelligent analysis of rule data */
};

struct IVA_METADATA_MGR_HEAD_S //tagIVAMetaDataMgrHead
{
	UCHAR 			ucMetaDataType;		/* metadata type	META_DATA_TYPE_E */
	USHORT			usMDHeadLenth;		/* metadata head length */
	USHORT			usMDBodyLenth;		/* metadata body lenth */
	UCHAR			ucProtocolVer;			/* protocol version：1,2,3... */
	UCHAR			ucMDataBodyVer;		/* metadata body version: 1,2,3... */
    UCHAR			ucExtHeadFlag:1;		/* carry extended data */
    UCHAR			ucExtDataLenth:7;		/* ExtData length */
	UCHAR			ucMDReserve[8];		/* reserve */
};				/* Structure size: 16 bytes in total */

/* Report coordinate point structure */
struct IVA_REPORT_COORD_S //tagIVAReportCoordInfo
{
    USHORT usX;        /* coordinate X  0-10000 */
    USHORT usY;        /* coordinate Y  0-10000 */
};

/* Reported information color type */
enum IVA_REPORT_COLOR_E // tagIVAReportColorType
{
    IVA_REPORT_COLOR_NONE = 0,      /* Colorless  does not require drawing */
	IVA_REPORT_COLOR_DEEP,          /* Dark color */
    IVA_REPORT_COLOR_LIGHT,         /* Light color */
    IVA_REPORT_COLOR_WHITE,        /* white */
    IVA_REPORT_COLOR_BLACK,        /* black */
    IVA_REPORT_COLOR_RED,          	/* red */
    IVA_REPORT_COLOR_GREEN,        /* green */
    IVA_REPORT_COLOR_BLUE,          /* blue */
    IVA_REPORT_COLOR_ORANGE,      /* orange */
    IVA_REPORT_COLOR_PURPLE,       /* purple */
    IVA_REPORT_COLOR_PINK,          /* pink */
    IVA_REPORT_COLOR_YELLOW,      /* yellow */
    IVA_REPORT_COLOR_BROWN,       /* brown */
    IVA_REPORT_COLOR_GRAY,         /* grey */
    IVA_REPORT_COLOR_BUT
};

/* Reported result information header */
struct IVA_REPORT_RESULT_HEAD_S //tagIVAReportResultHead
{
    USHORT	usChannel;              /* Channel number */
    UCHAR 	ucEnable;          	    /* Enabling state */
    USHORT usTargetNum;       	    /* target number */
    USHORT usPerTargetSz;			/* The size of each target */
    USHORT usDisplayHoldTime;  	    /* Continuous display time ms */
    UCHAR	ucExtHeadFlag:1;		/* if carry extended data */
    UCHAR	ucExtDataLenth:7;		/* Extended data length */
	UCHAR	ucMDReserve[8];		    /* reserve */
};		/* Structure size: 16 bytes in total */

/* Reported analysis results */
struct IVA_REPORT_RESULT_INFO_V1_S // tagIVAReportResultInfoV1
{
    ULONG ulTargetID;               	/* target ID */
    ULONG ulTrigRule;             		/* Trigger rule information 1 bit represents 1 rule */
    IVA_REPORT_COORD_S stStart;   		/* Target starting coordinates */
    IVA_REPORT_COORD_S stEnd;   		/* Target end coordinates */
    IVA_REPORT_COLOR_E	enLineColor; 	/* line color */
    ULONG ulLineAlpha; 					/* Line transparency 0-100 */
    ULONG ulTrailHoldTime;				/* Display time of historical trajectory points ms */
    ULONG ulAttachLen;					/* Retain length of ancillary information */
    ULONG ulAttachType; 				/* Retain Affiliated Information Types */
};

using IVA_REPORT_RESULT_INFO_S = IVA_REPORT_RESULT_INFO_V1_S;

/* Reporting Rule Type */
enum IVA_REPORT_RULE_TYPE_E // tagIVAReportRuleType
{
    IVA_REPORT_RULE_LINE = 0,   /* Linear rules (straight lines, polylines) */
    IVA_REPORT_RULE_POLY,      	/* Polygon rules (polygons, rectangles) */
    IVA_REPORT_RULE_BUT
};

/* Report Rule Trigger Type */
enum IVA_REPORT_TRIG_TYPE_E // tagIVAReportTrigType
{
    IVA_REPORT_TRIG_NONE = 0,      	/* No trigger type */ 
    IVA_REPORT_TRIG_BOTH,           /* Bidirectional triggering */
    IVA_REPORT_TRIG_CW,          	/* Clockwise trigger */
    IVA_REPORT_TRIG_CCW,            /* Counterclockwise trigger */
    IVA_REPORT_TRIG_IN,             /* Enter trigger */
    IVA_REPORT_TRIG_OUT,            /* Leaving trigger */
    IVA_REPORT_TRIG_BUT
};

/* Reported rule information header */
struct IVA_REPORT_RULE_HEAD_S // tagIVAReportRuleHead
{
    USHORT usChannel;            	 /* channel number */
    UCHAR 	ucEnable;              	 /* enable state */
    USHORT 	usRuleNum;            	 /* rule number */
    USHORT 	usPerTargetSz;			/* every targe size*/
    USHORT 	usDisplayHoldTime;   	 /* Rule duration display time ms */
    UCHAR	ucExtHeadFlag:1;		/* if carry extended data */
    UCHAR	ucExtDataLenth:7;		/* ext data length */
	UCHAR	ucMDReserve[6];		/* reserver */
};		/* Structure size: 16 bytes in total */

/* Reported rule information */
struct IVA_REPORT_RULE_INFO_V1_S // tagIVAReportRuleInfoV1
{
    ULONG ulRuleID;   								/* rule ID */
    ULONG bTriggered;  								/* Has the rule been triggered */
    IVA_REPORT_RULE_TYPE_E enRuleType;   			/* rule type */
    IVA_REPORT_TRIG_TYPE_E  enTrigType;  			/* Rule trigger type */
    IVA_REPORT_COORD_S astCoord[IVA_REPORT_COORD_NUM]; /* Regular coordinate points */
    ULONG ulCoordNum;  								/* Number of regular coordinate points */
	IVA_REPORT_COLOR_E	enLineColor; 				/* Rule Line Color */
	IVA_REPORT_COLOR_E	enFillColor; 				/* Rule Fill Color */
    ULONG ulLineAlpha; 								/* Line transparency 0-100 */
    ULONG ulFillAlpha; 									/* Fill Transparency 0-100 */
	ULONG ulAttachLen;								/* Retain length of ancillary information */
    ULONG ulAttachType; 								/* Retain Affiliated Information Types */ 
};

using IVA_REPORT_RULE_INFO_S = IVA_REPORT_RULE_INFO_V1_S;


#pragma pack()

// -------------------------------------------//

template<typename T>
auto parseResultType(util::BitStream<T> bs, size_t length)
    -> std::pair<util::BitStream<T>, IVA_REPORT_RESULT_HEAD_S>
{
    IVA_REPORT_RESULT_HEAD_S result_head;
    result_head.usChannel = bs.pop(16);
    result_head.ucEnable = bs.pop(8);
    result_head.usTargetNum = bs.pop(16);
    result_head.usPerTargetSz = bs.pop(16);
    result_head.usDisplayHoldTime = bs.pop(16);
    result_head.ucExtHeadFlag = bs.pop(1);
    result_head.ucExtDataLenth = bs.pop(7);
    std::copy(bs.position(), bs.position() + 8, result_head.ucMDReserve);
    bs.skip(8);

    if (result_head.ucExtHeadFlag) {
        bs.skip(result_head.ucExtDataLenth * 8);
        std::cout << "Skip extended data\n";
    }

    return {bs, result_head};
}

template<typename T>
void parsedtPayload(util::BitStream<T> bs)
{
    IVA_METADATA_MGR_HEAD_S header {};
    header.ucMetaDataType = bs.pop(8);
    header.usMDHeadLenth = bs.pop(16);
    header.usMDBodyLenth = bs.pop(16);
    header.ucProtocolVer = bs.pop(8);
    header.ucMDataBodyVer = bs.pop(8);
    header.ucExtHeadFlag = bs.pop(1);
    header.ucExtDataLenth = bs.pop(7);
    std::copy(bs.position(), bs.position() + 8, header.ucMDReserve);
    bs.skip(8);

    if (header.ucExtHeadFlag) {
        bs.skip(header.ucExtDataLenth * 8);
        fmt::println("Skip extended data");
    }

    switch (header.ucMetaDataType) {
    case tagMetaDataType::META_DATA_TYPE_RESULT_V2:
    {
        fmt::println("META_DATA_TYPE_RESULT_V2");
        fmt::println("usMDHeadLenth: {}", header.usMDHeadLenth);
        fmt::println("usMDBodyLenth: {}", header.usMDBodyLenth);
        fmt::println("ucProtocolVer: {}", header.ucProtocolVer);
        fmt::println("ucMDataBodyVer: {}", header.ucMDataBodyVer);
        auto [bs, result_head] = parseResultType(bs, header.usMDHeadLenth);
        fmt::println("usChannel: {}", result_head.usChannel);
        fmt::println("ucEnable: {}", result_head.ucEnable);
        fmt::println("usTargetNum: {}", result_head.usPerTargetSz);
        fmt::println("usDisplayHoldTime: {}", result_head.usDisplayHoldTime);
        
        //auto [bs, result] = parsedtPayload(bs);
        break;
    }
    case tagMetaDataType::META_DATA_TYPE_RULE_V2:
        std::cout << "META_DATA_TYPE_RULE_V2\n";
        break;
    default:
        std::cout << "unknown type\n";
        break;
    }
}

}
