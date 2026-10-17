#include "TextBook.h"
#include "types.h"
#include <regex>
#include <vector>


// 候选标题行最大长度（超过则认为是正文，不参与章节识别）
#define MAX_TITLE_LINE_LEN      80
// 自动识别模式下，预置模式最少命中次数，低于该值则回退到默认规则
#define AUTO_MIN_CHAPTER_HITS   3

// 预置目录模式（按优先级排序，命中数相同时靠前者优先）
static const wchar_t* s_preset_patterns[] =
{
    // 第X章/卷/部/节/回/集/篇（X 为半角/全角数字或中文数字，序号中允许空格，但至少含一个数字字符）
    LR"(^第(?=[0-9０-９零一二三四五六七八九十百千万两壹贰叁肆伍陆柒捌玖拾佰仟萬億 ]*[0-9０-９零一二三四五六七八九十百千万两壹贰叁肆伍陆柒捌玖拾佰仟萬億])[0-9０-９零一二三四五六七八九十百千万两壹贰叁肆伍陆柒捌玖拾佰仟萬億 ]{1,12}[章节卷部回集篇]($|[ 　：:、·．.\-—_(\[【（「].{0,70}$))",
    // 特殊篇名
    LR"(^(楔子|序章|序言|前言|引子|尾声|终章|后记)([ 　：:、·].*)?$)",
    // 番外
    LR"(^番外.{0,30}$)",
    // Chapter N（icase）
    LR"(^chapter\s+\d{1,6}([\s:：、·．.\-—].*)?$)",
    // 1、标题 / 1.标题（分隔符后不能以数字开头，避免误命中日期行）
    LR"(^\d{1,5}[、．.][ 　]?[^\d\s].{0,60}$)",
    // 001 标题
    LR"(^\d{1,5}[ 　]{1,4}\S.{1,60}$)",
    // 卷X / 部X
    LR"(^[卷部][ 　]?[0-9一二三四五六七八九十百千万两]{1,8}([ 　：:、·].*)?$)",
};
#define PRESET_PATTERN_COUNT    (sizeof(s_preset_patterns)/sizeof(s_preset_patterns[0]))

// 预置模式可能的首字符（与 s_preset_patterns 保持一致，用于廉价预过滤，
// 避免对正文短行执行正则匹配）
static BOOL IsTitleLeadingChar(wchar_t c)
{
    if (c >= _T('0') && c <= _T('9'))
        return TRUE;
    if (c == _T('c') || c == _T('C'))
        return TRUE;
    switch (c)
    {
    case _T('第'): case _T('楔'): case _T('序'): case _T('前'):
    case _T('引'): case _T('尾'): case _T('终'): case _T('后'):
    case _T('番'): case _T('卷'): case _T('部'):
        return TRUE;
    }
    return FALSE;
}

wchar_t TextBook::m_ValidChapter[] =
{
    _T(' '), _T('\t'),
    _T('0'), _T('1'), _T('2'), _T('3'), _T('4'),
    _T('5'), _T('6'), _T('7'), _T('8'), _T('9'),
    0xFF10, 0xFF11, 0xFF12, 0xFF13, 0xFF14, // 全角数字 ０-９
    0xFF15, 0xFF16, 0xFF17, 0xFF18, 0xFF19,
    _T('零'), _T('一'), _T('二'), _T('三'), _T('四'),
    _T('五'), _T('六'), _T('七'), _T('八'), _T('九'),
    _T('十'), _T('百'), _T('千'), _T('万'), _T('亿'),
    _T('壹'), _T('贰'), _T('叁'), _T('肆'),
    _T('伍'), _T('陆'), _T('柒'), _T('捌'), _T('玖'),
    _T('拾'), _T('佰'), _T('仟'), _T('萬'), _T('億'),
    _T('两'),
    0x3000
};

TextBook::TextBook()
{
}

TextBook::~TextBook()
{
    ForceKill();
}

book_type_t TextBook::GetBookType(void)
{
    return book_text;
}

BOOL TextBook::SaveBook(HWND hWnd)
{
    FILE *fp = NULL;

    fp = _tfopen(m_fileName, _T("wb"));
    if (!fp)
        return FALSE;

    fwrite("\xff\xfe", 2, 1, fp);
    fwrite(m_Text, sizeof(wchar_t), m_Length, fp);
    fclose(fp);

    return TRUE;
}

BOOL TextBook::UpdateChapters(int offset)
{
    chapters_t::iterator itor;
    
    for (itor=m_Chapters.begin(); itor!=m_Chapters.end(); itor++)
    {
        if (itor->index > m_Index)
        {
            itor->index += offset;
            if (itor->index < 0)
                itor->index = 0;
        }
    }
    return TRUE;
}

BOOL TextBook::ParserBook(HWND hWnd)
{
    BOOL ret = FALSE;

    if (!ReadBook())
        goto end;

    if (!ParserChapters())
        goto end;

    ret = TRUE;

end:
    if (!ret)
        CloseBook();

    return ret;
}

BOOL TextBook::ReadBook(void)
{
    FILE *fp = NULL;
    char *buf = NULL;
    int len;
    int size;
    BOOL ret = FALSE;

    if (m_Data && m_Size > 0)
    {
        buf = m_Data;
        len = m_Size;
    }
    else if (m_fileName[0])
    {
        fp = _tfopen(m_fileName, _T("rb"));
        if (!fp)
            goto end;

        fseek(fp, 0, SEEK_END);
        len = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        buf = (char *)malloc(len + 2);
        buf[len] = 0;
        buf[len+1] = 0;
        if (!buf)
            goto end;
        size = (int)fread(buf, 1, len, fp);
        if (size != len)
            goto end;
    }
    else
    {
        goto end;
    }

    if (!DecodeText(buf, len, &m_Text, &m_Length))
        goto end;

    if (m_bForceKill)
        goto end;

    ret = TRUE;

end:
    if (fp)
        fclose(fp);
    if (buf)
        free(buf);
    if (m_Data)
        m_Data = NULL;
    m_Size = 0;

    return ret;
}

BOOL TextBook::ParserChapters(void)
{
    if (m_Rule)
    {
        m_Chapters.clear();
        if (m_Rule->rule == 0)
        {
            return ParserChaptersAuto();
        }
        else if (m_Rule->rule == 1)
        {
            return ParserChaptersKeyword();
        }
        else if (m_Rule->rule == 2)
        {
            return ParserChaptersRegex();
        }
    }
    return FALSE;
}

BOOL TextBook::ParserChaptersDefault(void)
{
    wchar_t *text = m_Text;
    wchar_t title[MAX_CHAPTER_LENGTH] = { 0 };
    int line_size;
    int prefix_blank = 0;
    int title_len = 0;
    chapter_item_t chapter;

    while (TRUE)
    {
        if (m_bForceKill)
        {
            return FALSE;
        }

        if (!GetLine(text, m_Length - (int)(text - m_Text), &line_size, NULL, NULL, &prefix_blank, NULL))
        {
            break;
        }

        int content_len = line_size - prefix_blank;
        wchar_t *line = text + prefix_blank;
        int idx_1 = -1;

        // 标题应独占短行，过长行直接跳过（过滤正文假命中）
        if (content_len > 0 && content_len <= MAX_TITLE_LINE_LEN)
        {
            // 去掉行尾空白
            while (content_len > 0 && is_blank(line[content_len - 1]))
            {
                content_len--;
            }

            if (IsSpecialTitle(line, content_len))
            {
                idx_1 = 0;
            }
            else
            {
                BOOL valid = FALSE;

                // “第”必须靠近行首（允许少量前导字符，如 【( 等）
                for (int i = 0; i < content_len && i < 5; i++)
                {
                    if (line[i] == _T('第'))
                    {
                        idx_1 = i;
                        break;
                    }
                }
                if (idx_1 >= 0)
                {
                    for (int i = idx_1 + 1; i < content_len; i++)
                    {
                        wchar_t c = line[i];
                        if (c == _T('卷') || c == _T('章') || c == _T('部')
                            || c == _T('节') || c == _T('回') || c == _T('集'))
                        {
                            // “章”后必须为行尾或分隔符，序号区字符必须全部合法
                            if ((i + 1 >= content_len || IsTitleSeparator(line[i + 1]))
                                && IsChapter(line + idx_1 + 1, i - idx_1 - 1))
                            {
                                valid = TRUE;
                            }
                            break;
                        }
                        if (!IsChapter(line + i, 1))
                        {
                            break; // 序号区出现非法字符
                        }
                    }
                }
                if (!valid)
                {
                    idx_1 = -1;
                }
            }

            if (idx_1 >= 0)
            {
                title_len = content_len - idx_1 < (MAX_CHAPTER_LENGTH - 1) ? content_len - idx_1 : MAX_CHAPTER_LENGTH - 1;
                memcpy(title, line + idx_1, title_len * sizeof(wchar_t));
                title[title_len] = 0;

                chapter.index = (int)(text - m_Text);
                chapter.title = title;
                chapter.title_len = title_len;
                m_Chapters.push_back(chapter);
            }
        }

        // set index
        text += line_size + 1; // add 0x0a
    }

    return TRUE;
}

BOOL TextBook::ParserChaptersKeyword(void)
{
    wchar_t *text = m_Text;
    wchar_t title[MAX_CHAPTER_LENGTH] = { 0 };
    int line_size;
    int title_len = 0;
    BOOL bFound = FALSE;
    int idx_1 = -1;
    int cmplen;
    chapter_item_t chapter;

    while (TRUE)
    {
        if (m_bForceKill)
        {
            return FALSE;
        }

        if (!GetLine(text, m_Length - (int)(text - m_Text), &line_size, NULL, NULL, NULL, NULL))
        {
            break;
        }

        // check format
        cmplen = (int)wcslen(m_Rule->keyword);
        if (cmplen <= line_size)
        {
            bFound = FALSE;
            idx_1 = -1;
            for (int i = 0; i < line_size; i++)
            {
                if (wcsncmp(text+i, m_Rule->keyword, cmplen) == 0)
                {
                    idx_1 = i;
                    bFound = TRUE;
                    break;
                }
            }
            if (bFound)
            {
                title_len = line_size - idx_1 < (MAX_CHAPTER_LENGTH - 1) ? line_size - idx_1 : MAX_CHAPTER_LENGTH - 1;
                memcpy(title, text + idx_1, title_len * sizeof(wchar_t));
                title[title_len] = 0;

                chapter.index = /*idx_1 +*/ (int)(text - m_Text);
                chapter.title = title;
                chapter.title_len = title_len;
                m_Chapters.push_back(chapter);
            }
        }

        // set index
        text += line_size + 1; // add 0x0a
    }
    return TRUE;
}

BOOL TextBook::ParserChaptersRegex(void)
{
    wchar_t title[MAX_CHAPTER_LENGTH] = { 0 };
    int title_len = 0;
    chapter_item_t chapter;
    int offset = 0;
    std::wcmatch cm;
    std::wregex *e = NULL;
    TCHAR *text = m_Text;

    try
    {
        e = new std::wregex(m_Rule->regex);    
    }
    catch (...)
    {
        if (e)
        {
            delete e;
        }
        return FALSE;
    }

    while (std::regex_search(text, cm, *e, std::regex_constants::format_first_only))
    {
        if (m_bForceKill)
        {
            break;
        }

        title_len = (int)cm.length() < (MAX_CHAPTER_LENGTH - 1) ? (int)cm.length() : MAX_CHAPTER_LENGTH - 1;
        memcpy(title, cm.str().c_str(), title_len * sizeof(wchar_t));
        title[title_len] = 0;

        chapter.index = offset + (int)cm.position();
        chapter.title = title;
        chapter.title_len = title_len;
        m_Chapters.push_back(chapter);


        text += cm.position() + cm.length();
        offset += (int)cm.position() + (int)cm.length();
    }
    if (e)
    {
        delete e;
    }

    return TRUE;
}

BOOL TextBook::ParserChaptersAuto(void)
{
    std::vector<std::wregex> patterns;
    int hits[PRESET_PATTERN_COUNT] = { 0 };
    wchar_t *text = m_Text;
    int line_size;
    int prefix_blank = 0;

    // 编译预置模式，编译失败则回退到默认规则
    try
    {
        for (int i = 0; i < (int)PRESET_PATTERN_COUNT; i++)
        {
            // 第 4 条（Chapter N）忽略大小写
            patterns.push_back(std::wregex(s_preset_patterns[i],
                i == 3 ? std::regex_constants::icase : std::regex_constants::ECMAScript));
        }
    }
    catch (...)
    {
        return ParserChaptersDefault();
    }

    // 第一遍扫描：统计各模式命中数
    while (TRUE)
    {
        if (m_bForceKill)
        {
            return FALSE;
        }

        if (!GetLine(text, m_Length - (int)(text - m_Text), &line_size, NULL, NULL, &prefix_blank, NULL))
        {
            break;
        }

        int content_len = line_size - prefix_blank;
        if (content_len > 0 && content_len <= MAX_TITLE_LINE_LEN)
        {
            std::wstring line(text + prefix_blank, content_len);
            while (!line.empty() && is_blank(line[line.length() - 1]))
            {
                line.erase(line.length() - 1);
            }
            if (!line.empty() && IsTitleLeadingChar(line[0]))
            {
                for (int i = 0; i < (int)PRESET_PATTERN_COUNT; i++)
                {
                    if (std::regex_match(line, patterns[i]))
                    {
                        hits[i]++;
                    }
                }
            }
        }

        text += line_size + 1; // add 0x0a
    }

    // 选出命中数最多且达到阈值的模式；命中数相同按优先级（数组顺序）取靠前者
    int best = -1;
    for (int i = 0; i < (int)PRESET_PATTERN_COUNT; i++)
    {
        if (hits[i] >= AUTO_MIN_CHAPTER_HITS && (best < 0 || hits[i] > hits[best]))
        {
            best = i;
        }
    }
    if (best < 0)
    {
        return ParserChaptersDefault(); // 回退到增强的默认规则
    }

    // 第二遍扫描：用胜出的模式收集章节
    text = m_Text;
    while (TRUE)
    {
        if (m_bForceKill)
        {
            return FALSE;
        }

        if (!GetLine(text, m_Length - (int)(text - m_Text), &line_size, NULL, NULL, &prefix_blank, NULL))
        {
            break;
        }

        int content_len = line_size - prefix_blank;
        if (content_len > 0 && content_len <= MAX_TITLE_LINE_LEN)
        {
            std::wstring line(text + prefix_blank, content_len);
            while (!line.empty() && is_blank(line[line.length() - 1]))
            {
                line.erase(line.length() - 1);
            }
            if (!line.empty() && IsTitleLeadingChar(line[0]) && std::regex_match(line, patterns[best]))
            {
                chapter_item_t chapter;
                int title_len = (int)line.length() < (MAX_CHAPTER_LENGTH - 1) ? (int)line.length() : MAX_CHAPTER_LENGTH - 1;

                // 相邻重复标题去重（部分文本标题会打印两次）
                if (m_Chapters.empty() || m_Chapters.back().title != line)
                {
                    chapter.index = (int)(text - m_Text);
                    chapter.title = line.substr(0, title_len);
                    chapter.title_len = title_len;
                    m_Chapters.push_back(chapter);
                }
            }
        }

        text += line_size + 1; // add 0x0a
    }

    return TRUE;
}

BOOL TextBook::IsSpecialTitle(wchar_t* text, int len)
{
    static const wchar_t* words[] =
    {
        _T("楔子"), _T("序章"), _T("序言"), _T("前言"), _T("引子"),
        _T("尾声"), _T("终章"), _T("后记"), _T("番外")
    };

    if (!text || len <= 0)
        return FALSE;

    // 单独的“序”
    if (len == 1 && text[0] == _T('序'))
        return TRUE;

    for (int i = 0; i < sizeof(words) / sizeof(words[0]); i++)
    {
        int wlen = (int)wcslen(words[i]);
        if (wlen > len)
            continue;
        if (wcsncmp(text, words[i], wlen) != 0)
            continue;
        // “番外”开头即认为有效（番外一、番外·xx 等）
        if (wcscmp(words[i], _T("番外")) == 0)
            return TRUE;
        if (wlen == len || IsTitleSeparator(text[wlen]))
            return TRUE;
    }
    return FALSE;
}

BOOL TextBook::IsTitleSeparator(wchar_t c)
{
    switch (c)
    {
    case _T(' '): case _T('\t'): case 0x3000: case 0xA0:
    case _T('：'): case _T(':'): case _T('、'): case _T('·'): case _T('．'):
    case _T('.'): case _T('-'): case _T('—'): case _T('_'):
    case _T('('): case _T('（'): case _T('['): case _T('【'): case _T('「'):
        return TRUE;
    }
    return FALSE;
}

BOOL TextBook::IsChapter(wchar_t* text, int len)
{
    BOOL bFound = FALSE;
    if (!text || len <= 0)
        return FALSE;

    for (int i = 0; i < len; i++)
    {
        bFound = FALSE;
        for (int j = 0; j < sizeof(m_ValidChapter)/sizeof(m_ValidChapter[0]); j++)
        {
            if (text[i] == m_ValidChapter[j])
            {
                bFound = TRUE;
                break;
            }
        }
        if (!bFound)
        {
            return FALSE;
        }
    }
    return TRUE;
}