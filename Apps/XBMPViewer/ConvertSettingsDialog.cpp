//==============================================================================
//
//  ConvertSettingsDialog.cpp
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "ConvertSettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QVBoxLayout>

//==============================================================================
//  IMPLEMENTATION
//==============================================================================

ConvertSettingsDialog::ConvertSettingsDialog(QWidget* pParent)
    : QDialog(pParent)
    , m_pPlatform(NULL)
    , m_pFormat(NULL)
    , m_pMipLevels(NULL)
    , m_pGenericCompression(NULL)
{
    setWindowTitle("Convert to XBMP");

    m_pPlatform = new QComboBox(this);
    m_pFormat = new QComboBox(this);
    m_pMipLevels = new QSpinBox(this);
    m_pMipLevels->setRange(0, 16);

    m_pGenericCompression = new QCheckBox("Generic compression", this);

    UpdatePlatformList();
    UpdateFormatsByPlatform("PC", FALSE);

    m_pPlatform->setCurrentIndex(0);
    m_pFormat->setCurrentIndex(2);

    connect(m_pGenericCompression, &QCheckBox::checkStateChanged,
            this, &ConvertSettingsDialog::OnGenericCompressionChanged);
    connect(m_pPlatform, QOverload<s32>::of(&QComboBox::currentIndexChanged),
            this, &ConvertSettingsDialog::OnPlatformChanged);

    QFormLayout* pFormLayout = new QFormLayout();
    pFormLayout->addRow("Platform", m_pPlatform);
    pFormLayout->addRow("Format", m_pFormat);
    pFormLayout->addRow("Mips", m_pMipLevels);
    pFormLayout->addRow(QString(), m_pGenericCompression);

    QDialogButtonBox* pButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(pButtons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(pButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout* pLayout = new QVBoxLayout(this);
    pLayout->addLayout(pFormLayout);
    pLayout->addWidget(pButtons);
}

//==============================================================================

QString ConvertSettingsDialog::GetPlatform(void) const
{
    return m_pPlatform->currentText();
}

//==============================================================================

QString ConvertSettingsDialog::GetFormat(void) const
{
    return m_pFormat->currentText();
}

//==============================================================================

s32 ConvertSettingsDialog::GetMipLevels(void) const
{
    return m_pMipLevels->value();
}

//==============================================================================

xbool ConvertSettingsDialog::GetGenericCompression(void) const
{
    return m_pGenericCompression->isChecked() ? TRUE : FALSE;
}

//==============================================================================

void ConvertSettingsDialog::OnGenericCompressionChanged(s32 State)
{
    (void)State;
    UpdatePlatformList();

    QString Platform = m_pPlatform->currentText();
    if (m_pGenericCompression->isChecked())
        UpdateFormatsByPlatform("PC", TRUE);
    else
        UpdateFormatsByPlatform(Platform, FALSE);
}

//==============================================================================

void ConvertSettingsDialog::OnPlatformChanged(s32 Index)
{
    (void)Index;
    const QString Platform = m_pPlatform->currentText();
    UpdateFormatsByPlatform(Platform, m_pGenericCompression->isChecked() ? TRUE : FALSE);
}

//==============================================================================

void ConvertSettingsDialog::UpdatePlatformList(void)
{
    const QString Current = m_pPlatform->currentText();

    m_pPlatform->clear();
    if (m_pGenericCompression->isChecked())
    {
        m_pPlatform->addItem("Native");
    }
    else
    {
        m_pPlatform->addItem("PC");
        m_pPlatform->addItem("Xbox");
        m_pPlatform->addItem("PS2");
        m_pPlatform->addItem("GameCube");
        m_pPlatform->addItem("Native");
    }

    const s32 Index = m_pPlatform->findText(Current);
    if (Index >= 0)
        m_pPlatform->setCurrentIndex(Index);
}

//==============================================================================

void ConvertSettingsDialog::UpdateFormatsByPlatform(const QString& Platform, xbool GenericCompression)
{
    m_pFormat->clear();

    if (Platform == "PC" || Platform == "Xbox")
    {
        m_pFormat->addItem("32_ARGB_8888");
        m_pFormat->addItem("32_URGB_8888");
        m_pFormat->addItem("24_RGB_888");
        m_pFormat->addItem("16_ARGB_1555");
        m_pFormat->addItem("16_URGB_1555");
        m_pFormat->addItem("16_RGB_565");
    }
    else if (Platform == "PS2")
    {
        m_pFormat->addItem("32_ABGR_8888");
        m_pFormat->addItem("32_UBGR_8888");
        m_pFormat->addItem("24_BGR_888");
        m_pFormat->addItem("16_ABGR_1555");
        m_pFormat->addItem("16_UBGR_1555");
        m_pFormat->addItem("P8_ABGR_8888");
        m_pFormat->addItem("P8_UBGR_8888");
        m_pFormat->addItem("P4_ABGR_8888");
        m_pFormat->addItem("P4_UBGR_8888");
    }
    else if (Platform == "GameCube")
    {
        m_pFormat->addItem("32_RGBA_8888");
        m_pFormat->addItem("32_RGBU_8888");
        m_pFormat->addItem("16_RGBA_4444");
        m_pFormat->addItem("16_RGBA_5551");
        m_pFormat->addItem("16_RGBU_5551");
        m_pFormat->addItem("16_RGB_565");
        m_pFormat->addItem("P8_RGBA_8888");
        m_pFormat->addItem("P8_RGBU_8888");
        m_pFormat->addItem("P4_RGBA_8888");
        m_pFormat->addItem("P4_RGBU_8888");
    }
    else if (Platform == "Native")
    {
        m_pFormat->addItem("32_ARGB_8888");
        m_pFormat->addItem("32_URGB_8888");
        m_pFormat->addItem("24_RGB_888");
        m_pFormat->addItem("16_ARGB_1555");
        m_pFormat->addItem("16_URGB_1555");
        m_pFormat->addItem("16_RGB_565");
        m_pFormat->addItem("32_ABGR_8888");
        m_pFormat->addItem("32_UBGR_8888");
        m_pFormat->addItem("24_BGR_888");
        m_pFormat->addItem("16_ABGR_1555");
        m_pFormat->addItem("16_UBGR_1555");
        m_pFormat->addItem("32_RGBA_8888");
        m_pFormat->addItem("32_RGBU_8888");
        m_pFormat->addItem("16_RGBA_4444");
        m_pFormat->addItem("16_RGBA_5551");
        m_pFormat->addItem("16_RGBU_5551");
    }

    if (GenericCompression)
    {
        m_pFormat->addItem("16_RGBA_4444");
        m_pFormat->addItem("16_ARGB_4444");
        m_pFormat->addItem("16_RGBA_5551");
        m_pFormat->addItem("16_RGBU_5551");
        m_pFormat->addItem("16_BGRA_4444");
        m_pFormat->addItem("16_ABGR_4444");
        m_pFormat->addItem("16_BGRA_5551");
        m_pFormat->addItem("16_BGRU_5551");
        m_pFormat->addItem("P8_RGB_888");
        m_pFormat->addItem("P8_RGBA_4444");
        m_pFormat->addItem("P8_ARGB_4444");
        m_pFormat->addItem("P8_ARGB_1555");
        m_pFormat->addItem("P8_URGB_1555");
        m_pFormat->addItem("P8_RGB_565");
        m_pFormat->addItem("P4_RGB_888");
        m_pFormat->addItem("P4_RGBA_4444");
        m_pFormat->addItem("P4_ARGB_4444");
        m_pFormat->addItem("P4_ARGB_1555");
        m_pFormat->addItem("P4_URGB_1555");
        m_pFormat->addItem("P4_RGB_565");
        m_pFormat->addItem("DXT1");
        m_pFormat->addItem("DXT2");
        m_pFormat->addItem("DXT3");
        m_pFormat->addItem("DXT4");
        m_pFormat->addItem("DXT5");
        m_pFormat->addItem("A8");
    }

    if (Platform == "PC" || Platform == "Xbox" || Platform == "Native")
        m_pFormat->setCurrentText("32_ARGB_8888");
    else if (Platform == "PS2")
        m_pFormat->setCurrentText("32_ABGR_8888");
    else if (Platform == "GameCube")
        m_pFormat->setCurrentText("32_RGBA_8888");
}
