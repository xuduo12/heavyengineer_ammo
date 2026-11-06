# -*- coding: utf-8 -*-
"""
VQF 算法详解 PDF 生成器

用途：读取 Library/Src/VQF_filter.c 与 Library/Inc/VQF_filter.h 的真实源码，
配上逐行解释（来自 vqf_content.py），渲染成 PDF。

代码字体使用 VSCode 当前的代码字体 Maple Mono CF CN。
中文字体使用微软雅黑。

运行：python docs/vqf_pdf.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_LEFT, TA_CENTER
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Table, TableStyle,
    KeepTogether, PageBreak, HRFlowable,
)

import vqf_content as CT

# --------------------------------------------------------------------------
# 路径
# --------------------------------------------------------------------------
ROOT = r'F:\astra\6\gimbal'
SRC_C = os.path.join(ROOT, r'Library\Src\VQF_filter.c')
SRC_H = os.path.join(ROOT, r'Library\Inc\VQF_filter.h')
OUT_PDF = os.path.join(ROOT, r'docs\VQF算法详解.pdf')

# --------------------------------------------------------------------------
# 字体注册
# --------------------------------------------------------------------------
FDIR = r'C:\Windows\Fonts'
UFONT = os.path.join(os.environ['LOCALAPPDATA'], r'Microsoft\Windows\Fonts')

# ---- 代码字体：Maple Mono CF CN（VSCode 当前使用的代码字体）----
MAPLE_R = os.path.join(UFONT, 'MapleMono-NF-CN-Regular.ttf')
MAPLE_B = os.path.join(UFONT, 'MapleMono-NF-CN-Bold.ttf')
if not os.path.exists(MAPLE_R):
    raise SystemExit('找不到 Maple Mono CF CN: %s' % MAPLE_R)

pdfmetrics.registerFont(TTFont('Maple', MAPLE_R))
pdfmetrics.registerFont(TTFont('Maple-Bold', MAPLE_B))
pdfmetrics.registerFontFamily('Maple', normal='Maple', bold='Maple-Bold',
                              italic='Maple', boldItalic='Maple-Bold')
# 兼容旧名字
CODE_FONT = 'Maple'
CODE_FONT_B = 'Maple-Bold'

# ---- 正文中文字体 ----
pdfmetrics.registerFont(TTFont('YaHei', os.path.join(FDIR, 'msyh.ttc'), subfontIndex=0))
pdfmetrics.registerFont(TTFont('YaHei-Bold', os.path.join(FDIR, 'msyhbd.ttc'), subfontIndex=0))
pdfmetrics.registerFontFamily('YaHei', normal='YaHei', bold='YaHei-Bold',
                              italic='YaHei', boldItalic='YaHei-Bold')

# ---- 符号回退字体：Maple Mono 与 微软雅黑 都缺数学符号，用 Cambria Math 补 ----
# cambria.ttc 的索引 1 是 Cambria Math，覆盖 √ ∝ ⊗ ⊥ ①②③④⑤ ᵀ ẑ ⁻ 等全部缺字。
pdfmetrics.registerFont(TTFont('Sym', os.path.join(FDIR, 'cambria.ttc'), subfontIndex=1))

# 这两个字体缺失、需要回退到 Sym 的字符（合并取并集）
SYM_SET = set('ᵀẑ√∝⊗⊥①②③④⑤⁻')


def sym(t):
    """把缺失的数学符号用 Cambria Math 渲染；并去掉组合用的抑扬符 U+0302。"""
    t = t.replace('\u0302', '')          # ω̂ → ω（组合符在 PDF 里会断行错位）
    out, i, n = [], 0, len(t)
    while i < n:
        if t[i] in SYM_SET:
            j = i
            while j < n and t[j] in SYM_SET:
                j += 1
            out.append('<font name="Sym">%s</font>' % t[i:j])
            i = j
        else:
            out.append(t[i])
            i += 1
    return ''.join(out)


def esc_sym(t):
    """先转义 XML，再做符号回退。用于代码等需要转义的文本。"""
    return sym(esc(t))
pdfmetrics.registerFontFamily('YaHei', normal='YaHei', bold='YaHei-Bold',
                              italic='YaHei', boldItalic='YaHei-Bold')

# --------------------------------------------------------------------------
# 颜色
# --------------------------------------------------------------------------
C_ACCENT = colors.HexColor('#2563EB')
C_CODE_BG = colors.HexColor('#F6F8FA')
C_EXPL_BG = colors.HexColor('#FFFFFF')
C_GUTTER = colors.HexColor('#9AA0A6')
C_EXPL_TXT = colors.HexColor('#3F3F46')
C_RULE = colors.HexColor('#D4D4D8')
C_WARN = colors.HexColor('#B45309')
C_GREEN = colors.HexColor('#15803D')
C_RED = colors.HexColor('#B91C1C')

# --------------------------------------------------------------------------
# 页面几何
# --------------------------------------------------------------------------
PW, PH = A4
LM = RM = 18 * mm
TM = 18 * mm
BM = 16 * mm
CONTENT_W = PW - LM - RM

# --------------------------------------------------------------------------
# 样式
# --------------------------------------------------------------------------
S = {}
S['title'] = ParagraphStyle('title', fontName='YaHei-Bold', fontSize=30, leading=40,
                            alignment=TA_CENTER, textColor=colors.HexColor('#111827'))
S['subtitle'] = ParagraphStyle('subtitle', fontName='YaHei', fontSize=13, leading=22,
                               alignment=TA_CENTER, textColor=colors.HexColor('#52525B'))
S['h1'] = ParagraphStyle('h1', fontName='YaHei-Bold', fontSize=20, leading=28,
                         textColor=C_ACCENT, spaceBefore=16, spaceAfter=8)
S['h2'] = ParagraphStyle('h2', fontName='YaHei-Bold', fontSize=15, leading=22,
                         textColor=colors.HexColor('#1F2937'), spaceBefore=12, spaceAfter=6)
S['h3'] = ParagraphStyle('h3', fontName='YaHei-Bold', fontSize=12.5, leading=19,
                         textColor=colors.HexColor('#374151'), spaceBefore=9, spaceAfter=4)
S['p'] = ParagraphStyle('p', fontName='YaHei', fontSize=10, leading=17,
                        textColor=colors.HexColor('#27272A'), spaceAfter=5, alignment=TA_LEFT)
S['bullet'] = ParagraphStyle('bullet', parent=S['p'], leftIndent=14, bulletIndent=4, spaceAfter=3)
S['code'] = ParagraphStyle('code', fontName=CODE_FONT, fontSize=8.2, leading=11.6,
                           textColor=colors.HexColor('#24292F'))
S['codeblk'] = ParagraphStyle('codeblk', fontName=CODE_FONT, fontSize=8.6, leading=12.4,
                              textColor=colors.HexColor('#24292F'))
S['expl'] = ParagraphStyle('expl', fontName='YaHei', fontSize=8.6, leading=13.4,
                           textColor=C_EXPL_TXT, leftIndent=11, spaceAfter=3.5,
                           borderPadding=0)
S['caption'] = ParagraphStyle('caption', fontName='YaHei', fontSize=8.6, leading=13,
                              textColor=colors.HexColor('#71717A'))
S['th'] = ParagraphStyle('th', fontName='YaHei-Bold', fontSize=9, leading=13.5,
                         textColor=colors.white)
S['td'] = ParagraphStyle('td', fontName='YaHei', fontSize=8.8, leading=13.5,
                         textColor=colors.HexColor('#27272A'))
S['tdc'] = ParagraphStyle('tdc', fontName=CODE_FONT, fontSize=8.4, leading=13.5,
                          textColor=colors.HexColor('#27272A'))

PAGE_W = [0]  # 页码计数


def esc(t):
    """转义 XML 特殊字符，供 Paragraph 使用。"""
    return (t.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;'))


def on_page(canv, doc):
    """页眉页脚（封面不绘制）。"""
    if doc.page == 1:
        return
    canv.saveState()
    # 页眉
    canv.setFont('YaHei', 7.6)
    canv.setFillColor(colors.HexColor('#9CA3AF'))
    canv.drawString(LM, PH - TM + 6 * mm, 'VQF 姿态滤波算法详解')
    canv.drawRightString(PW - RM, PH - TM + 6 * mm, 'gimbal / Library')
    canv.setStrokeColor(C_RULE)
    canv.setLineWidth(0.5)
    canv.line(LM, PH - TM + 4.4 * mm, PW - RM, PH - TM + 4.4 * mm)
    # 页脚
    canv.line(LM, BM - 3.4 * mm, PW - RM, BM - 3.4 * mm)
    canv.setFont('YaHei', 7.6)
    canv.drawString(LM, BM - 7 * mm, 'VQF_filter.c / VQF_filter.h 逐行解析')
    canv.drawRightString(PW - RM, BM - 7 * mm, '第 %d 页' % (doc.page - 1))
    canv.restoreState()


def code_block(text):
    """多行代码块。"""
    rows = []
    for ln in text.split('\n'):
        rows.append([Paragraph(esc_sym(ln) if ln.strip() else '&nbsp;', S['codeblk'])])
    t = Table(rows, colWidths=[CONTENT_W])
    t.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, -1), C_CODE_BG),
        ('LEFTPADDING', (0, 0), (-1, -1), 7),
        ('RIGHTPADDING', (0, 0), (-1, -1), 5),
        ('TOPPADDING', (0, 0), (-1, -1), 0.6),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 0.6),
        ('LINEBEFORE', (0, 0), (0, -1), 1.6, C_ACCENT),
        ('TOPPADDING', (0, 0), (-1, 0), 5),
        ('BOTTOMPADDING', (0, -1), (-1, -1), 5),
    ]))
    return t


def make_table(headers, rows):
    """对比表格。"""
    data = [[Paragraph(esc_sym(h), S['th']) for h in headers]]
    for r in rows:
        cells = []
        for i, c in enumerate(r):
            st = S['tdc'] if i == 0 else S['td']
            # 单元格里含有刻意的 HTML 标签（<b>），不能转义
            cells.append(Paragraph(sym(c), st))
        data.append(cells)
    n = len(headers)
    cw = [CONTENT_W * 0.24] + [CONTENT_W * 0.76 / (n - 1)] * (n - 1) if n > 1 else [CONTENT_W]
    t = Table(data, colWidths=cw, repeatRows=1)
    style = [
        ('BACKGROUND', (0, 0), (-1, 0), C_ACCENT),
        ('VALIGN', (0, 0), (-1, -1), 'TOP'),
        ('GRID', (0, 0), (-1, -1), 0.4, colors.HexColor('#D4D4D8')),
        ('LEFTPADDING', (0, 0), (-1, -1), 5),
        ('RIGHTPADDING', (0, 0), (-1, -1), 5),
        ('TOPPADDING', (0, 0), (-1, -1), 3.5),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 3.5),
    ]
    for i in range(1, len(data)):
        if i % 2 == 0:
            style.append(('BACKGROUND', (0, i), (-1, i), colors.HexColor('#F4F4F5')))
    t.setStyle(TableStyle(style))
    return t


def render_explained_source(path, expl, heading, intro):
    """把源码按行渲染，每行下面跟一条解释。"""
    flow = [Paragraph(heading, S['h1']), Paragraph(intro, S['p'])]
    with open(path, encoding='utf-8') as fh:
        lines = fh.read().split('\n')
    # 记录每个"函数/代码段"分组，便于分页时保持连续
    for idx, raw in enumerate(lines, start=1):
        e = expl.get(idx)
        gut = '%4d' % idx
        code_txt = esc_sym(raw) if raw.strip() else '&nbsp;'
        cp = Paragraph('<font color="#9AA0A6">%s</font>  %s' % (gut, code_txt), S['code'])
        t = Table([[cp]], colWidths=[CONTENT_W])
        t.setStyle(TableStyle([
            ('BACKGROUND', (0, 0), (-1, -1), C_CODE_BG),
            ('LEFTPADDING', (0, 0), (-1, -1), 5),
            ('RIGHTPADDING', (0, 0), (-1, -1), 4),
            ('TOPPADDING', (0, 0), (-1, -1), 0.8),
            ('BOTTOMPADDING', (0, 0), (-1, -1), 0.8),
            ('LINEBEFORE', (0, 0), (0, -1), 1.5, colors.HexColor('#A1A1AA')),
        ]))
        if e:
            # 解释文本里含有刻意的 HTML 标签（<b>/<code>），不能转义
            flow.append(KeepTogether([t, Paragraph(sym(e), S['expl'])]))
        else:
            flow.append(t)
    return flow


def build():
    doc = BaseDocTemplate(
        OUT_PDF, pagesize=A4,
        leftMargin=LM, rightMargin=RM, topMargin=TM, bottomMargin=BM,
        title='VQF 姿态滤波算法详解', author='gimbal',
    )
    frame = Frame(LM, BM, CONTENT_W, PH - TM - BM, id='body',
                  leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
    doc.addPageTemplates([PageTemplate(id='main', frames=[frame], onPage=on_page)])

    F = []
    for kind, payload in CT.BUILD:
        if kind == 'title':
            F.append(Spacer(1, 55 * mm))
            F.append(Paragraph(esc_sym(payload), S['title']))
            F.append(Spacer(1, 6 * mm))
        elif kind == 'subtitle':
            F.append(Paragraph(esc_sym(payload), S['subtitle']))
        elif kind == 'h1':
            F.append(Paragraph(esc_sym(payload), S['h1']))
            F.append(HRFlowable(width='100%', thickness=1.1, color=C_ACCENT,
                                spaceBefore=1, spaceAfter=7))
        elif kind == 'h2':
            F.append(Paragraph(esc_sym(payload), S['h2']))
        elif kind == 'h3':
            F.append(Paragraph(esc_sym(payload), S['h3']))
        elif kind == 'p':
            F.append(Paragraph(sym(payload), S['p']))
        elif kind == 'bullet':
            for it in payload:
                F.append(Paragraph(sym(it), S['bullet'], bulletText='•'))
            F.append(Spacer(1, 3))
        elif kind == 'code':
            F.append(Spacer(1, 3))
            F.append(code_block(payload))
            F.append(Spacer(1, 6))
        elif kind == 'table':
            F.append(Spacer(1, 3))
            F.append(make_table(payload[0], payload[1]))
            F.append(Spacer(1, 7))
        elif kind == 'pagebreak':
            F.append(PageBreak())
        elif kind == 'src_c':
            F.extend(render_explained_source(SRC_C, CT.C_EXPL, CT.C_HEADING, CT.C_INTRO))
        elif kind == 'src_h':
            F.extend(render_explained_source(SRC_H, CT.H_EXPL, CT.H_HEADING, CT.H_INTRO))
        else:
            raise ValueError('unknown block: %r' % kind)

    doc.build(F)
    print('OK ->', OUT_PDF)


if __name__ == '__main__':
    build()
