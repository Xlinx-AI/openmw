#include "proceduralgeneratordialog.hpp"

#include <chrono>
#include <random>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>

#include "../../model/doc/document.hpp"
#include "../../model/procs/proceduralgenerator.hpp"
#include "../../model/procs/proceduralstate.hpp"
#include "../../model/procs/worldanalyzer.hpp"

namespace CSVProcs
{
    ProceduralGeneratorDialog::ProceduralGeneratorDialog(CSMDoc::Document& document, QWidget* parent)
        : QDialog(parent)
        , mDocument(document)
    {
        setWindowTitle(tr("Procedural World Generator"));
        setMinimumSize(600, 700);
        resize(700, 800);
        
        mGenerator = std::make_unique<CSMProcs::ProceduralGenerator>(document);
        mGenerator->setProgressCallback([this](int current, int total, const std::string& message) {
            QMetaObject::invokeMethod(this, "onProgress", Qt::QueuedConnection,
                Q_ARG(int, current), Q_ARG(int, total), Q_ARG(QString, QString::fromStdString(message)));
        });
        
        setupUi();
        loadStateToUi();
    }
    
    ProceduralGeneratorDialog::~ProceduralGeneratorDialog() = default;
    
    void ProceduralGeneratorDialog::setupUi()
    {
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        
        // Tab widget for organized parameters
        mTabWidget = new QTabWidget(this);
        mainLayout->addWidget(mTabWidget, 1);
        
        createGeneralTab();
        createTerrainTab();
        createObjectsTab();
        createInteriorsTab();
        createAdvancedTab();
        
        // Progress section
        QGroupBox* progressGroup = new QGroupBox(tr("Progress"), this);
        QVBoxLayout* progressLayout = new QVBoxLayout(progressGroup);
        
        mProgressBar = new QProgressBar(this);
        mProgressBar->setRange(0, 100);
        mProgressBar->setValue(0);
        progressLayout->addWidget(mProgressBar);
        
        mStatusLabel = new QLabel(tr("Ready"), this);
        progressLayout->addWidget(mStatusLabel);
        
        mainLayout->addWidget(progressGroup);
        
        // Buttons
        QHBoxLayout* buttonLayout = new QHBoxLayout();
        
        mPreviewButton = new QPushButton(tr("Preview Cell"), this);
        connect(mPreviewButton, &QPushButton::clicked, this, &ProceduralGeneratorDialog::onPreview);
        buttonLayout->addWidget(mPreviewButton);
        
        buttonLayout->addStretch();
        
        mCancelButton = new QPushButton(tr("Cancel"), this);
        mCancelButton->setEnabled(false);
        connect(mCancelButton, &QPushButton::clicked, this, &ProceduralGeneratorDialog::onCancel);
        buttonLayout->addWidget(mCancelButton);
        
        mGenerateButton = new QPushButton(tr("Generate World"), this);
        mGenerateButton->setDefault(true);
        connect(mGenerateButton, &QPushButton::clicked, this, &ProceduralGeneratorDialog::onGenerate);
        buttonLayout->addWidget(mGenerateButton);
        
        QPushButton* closeButton = new QPushButton(tr("Close"), this);
        connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
        buttonLayout->addWidget(closeButton);
        
        mainLayout->addLayout(buttonLayout);
    }
    
    void ProceduralGeneratorDialog::createGeneralTab()
    {
        QWidget* tab = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(tab);
        
        // World Size Group
        QGroupBox* sizeGroup = new QGroupBox(tr("World Size"), tab);
        QGridLayout* sizeLayout = new QGridLayout(sizeGroup);
        
        sizeLayout->addWidget(new QLabel(tr("Width (cells):"), sizeGroup), 0, 0);
        mWorldSizeX = new QSpinBox(sizeGroup);
        mWorldSizeX->setRange(1, 1000);
        mWorldSizeX->setValue(10);
        sizeLayout->addWidget(mWorldSizeX, 0, 1);
        
        sizeLayout->addWidget(new QLabel(tr("Height (cells):"), sizeGroup), 0, 2);
        mWorldSizeY = new QSpinBox(sizeGroup);
        mWorldSizeY->setRange(1, 1000);
        mWorldSizeY->setValue(10);
        sizeLayout->addWidget(mWorldSizeY, 0, 3);
        
        sizeLayout->addWidget(new QLabel(tr("Origin X:"), sizeGroup), 1, 0);
        mOriginX = new QSpinBox(sizeGroup);
        mOriginX->setRange(-10000, 10000);
        mOriginX->setValue(0);
        sizeLayout->addWidget(mOriginX, 1, 1);
        
        sizeLayout->addWidget(new QLabel(tr("Origin Y:"), sizeGroup), 1, 2);
        mOriginY = new QSpinBox(sizeGroup);
        mOriginY->setRange(-10000, 10000);
        mOriginY->setValue(0);
        sizeLayout->addWidget(mOriginY, 1, 3);
        
        layout->addWidget(sizeGroup);
        
        // Seed Group
        QGroupBox* seedGroup = new QGroupBox(tr("Random Seed"), tab);
        QHBoxLayout* seedLayout = new QHBoxLayout(seedGroup);
        
        mSeedEdit = new QLineEdit(seedGroup);
        mSeedEdit->setPlaceholderText(tr("Enter seed or leave empty for random"));
        seedLayout->addWidget(mSeedEdit, 1);
        
        mRandomSeedButton = new QPushButton(tr("Random"), seedGroup);
        connect(mRandomSeedButton, &QPushButton::clicked, this, &ProceduralGeneratorDialog::onRandomSeed);
        seedLayout->addWidget(mRandomSeedButton);
        
        layout->addWidget(seedGroup);
        
        // Reference Group
        QGroupBox* refGroup = new QGroupBox(tr("Reference Worldspace (Style Learning)"), tab);
        QVBoxLayout* refLayout = new QVBoxLayout(refGroup);
        
        mUseReference = new QCheckBox(tr("Learn style from reference worldspace"), refGroup);
        connect(mUseReference, &QCheckBox::stateChanged, this, &ProceduralGeneratorDialog::onUseReferenceChanged);
        refLayout->addWidget(mUseReference);
        
        QHBoxLayout* refInputLayout = new QHBoxLayout();
        refInputLayout->addWidget(new QLabel(tr("Reference:"), refGroup));
        mReferenceWorldspace = new QLineEdit(refGroup);
        mReferenceWorldspace->setPlaceholderText(tr("e.g., #-2,-2 to #5,5 for Morrowind area"));
        mReferenceWorldspace->setEnabled(false);
        connect(mReferenceWorldspace, &QLineEdit::textChanged, this, &ProceduralGeneratorDialog::onReferenceChanged);
        refInputLayout->addWidget(mReferenceWorldspace, 1);
        
        mAnalyzeButton = new QPushButton(tr("Analyze"), refGroup);
        mAnalyzeButton->setEnabled(false);
        connect(mAnalyzeButton, &QPushButton::clicked, this, &ProceduralGeneratorDialog::onAnalyzeReference);
        refInputLayout->addWidget(mAnalyzeButton);
        
        refLayout->addLayout(refInputLayout);
        
        QLabel* refHint = new QLabel(tr("Tip: Use cell coordinate prefix like '#' to analyze all exterior cells,\n"
                                        "or specify a range to analyze a specific area."), refGroup);
        refHint->setStyleSheet("color: gray; font-size: 10px;");
        refLayout->addWidget(refHint);
        
        layout->addWidget(refGroup);
        
        // Options Group
        QGroupBox* optionsGroup = new QGroupBox(tr("Generation Options"), tab);
        QVBoxLayout* optionsLayout = new QVBoxLayout(optionsGroup);
        
        mGenerateExteriors = new QCheckBox(tr("Generate exterior terrain and objects"), optionsGroup);
        mGenerateExteriors->setChecked(true);
        optionsLayout->addWidget(mGenerateExteriors);
        
        mGenerateInteriors = new QCheckBox(tr("Generate interior cells"), optionsGroup);
        mGenerateInteriors->setChecked(true);
        optionsLayout->addWidget(mGenerateInteriors);
        
        mGeneratePathgrids = new QCheckBox(tr("Generate pathgrids for AI navigation"), optionsGroup);
        mGeneratePathgrids->setChecked(true);
        optionsLayout->addWidget(mGeneratePathgrids);
        
        mOverwriteExisting = new QCheckBox(tr("Overwrite existing cells (use with caution!)"), optionsGroup);
        mOverwriteExisting->setChecked(false);
        optionsLayout->addWidget(mOverwriteExisting);
        
        layout->addWidget(optionsGroup);
        
        layout->addStretch();
        mTabWidget->addTab(tab, tr("General"));
    }
    
    void ProceduralGeneratorDialog::createTerrainTab()
    {
        QScrollArea* scrollArea = new QScrollArea();
        QWidget* tab = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(tab);
        
        // Height Settings
        QGroupBox* heightGroup = new QGroupBox(tr("Height Settings"), tab);
        QGridLayout* heightLayout = new QGridLayout(heightGroup);
        
        heightLayout->addWidget(new QLabel(tr("Base Height:"), heightGroup), 0, 0);
        mBaseHeight = new QDoubleSpinBox(heightGroup);
        mBaseHeight->setRange(-10000, 10000);
        mBaseHeight->setValue(0);
        heightLayout->addWidget(mBaseHeight, 0, 1);
        
        heightLayout->addWidget(new QLabel(tr("Height Variation:"), heightGroup), 0, 2);
        mHeightVariation = new QDoubleSpinBox(heightGroup);
        mHeightVariation->setRange(0, 50000);
        mHeightVariation->setValue(1000);
        heightLayout->addWidget(mHeightVariation, 0, 3);
        
        layout->addWidget(heightGroup);
        
        // Noise Settings
        QGroupBox* noiseGroup = new QGroupBox(tr("Terrain Noise"), tab);
        QGridLayout* noiseLayout = new QGridLayout(noiseGroup);
        
        noiseLayout->addWidget(new QLabel(tr("Roughness:"), noiseGroup), 0, 0);
        mRoughness = new QSlider(Qt::Horizontal, noiseGroup);
        mRoughness->setRange(0, 100);
        mRoughness->setValue(50);
        noiseLayout->addWidget(mRoughness, 0, 1);
        mRoughnessLabel = new QLabel("0.50", noiseGroup);
        mRoughnessLabel->setMinimumWidth(40);
        noiseLayout->addWidget(mRoughnessLabel, 0, 2);
        connect(mRoughness, &QSlider::valueChanged, [this](int v) {
            mRoughnessLabel->setText(QString::number(v / 100.0, 'f', 2));
        });
        
        noiseLayout->addWidget(new QLabel(tr("Erosion:"), noiseGroup), 1, 0);
        mErosion = new QSlider(Qt::Horizontal, noiseGroup);
        mErosion->setRange(0, 100);
        mErosion->setValue(30);
        noiseLayout->addWidget(mErosion, 1, 1);
        mErosionLabel = new QLabel("0.30", noiseGroup);
        mErosionLabel->setMinimumWidth(40);
        noiseLayout->addWidget(mErosionLabel, 1, 2);
        connect(mErosion, &QSlider::valueChanged, [this](int v) {
            mErosionLabel->setText(QString::number(v / 100.0, 'f', 2));
        });
        
        noiseLayout->addWidget(new QLabel(tr("Octaves:"), noiseGroup), 2, 0);
        mOctaves = new QSpinBox(noiseGroup);
        mOctaves->setRange(1, 12);
        mOctaves->setValue(6);
        noiseLayout->addWidget(mOctaves, 2, 1, 1, 2);
        
        noiseLayout->addWidget(new QLabel(tr("Persistence:"), noiseGroup), 3, 0);
        mPersistence = new QDoubleSpinBox(noiseGroup);
        mPersistence->setRange(0.1, 1.0);
        mPersistence->setSingleStep(0.05);
        mPersistence->setValue(0.5);
        noiseLayout->addWidget(mPersistence, 3, 1, 1, 2);
        
        noiseLayout->addWidget(new QLabel(tr("Lacunarity:"), noiseGroup), 4, 0);
        mLacunarity = new QDoubleSpinBox(noiseGroup);
        mLacunarity->setRange(1.0, 4.0);
        mLacunarity->setSingleStep(0.1);
        mLacunarity->setValue(2.0);
        noiseLayout->addWidget(mLacunarity, 4, 1, 1, 2);
        
        layout->addWidget(noiseGroup);
        
        // Features Group
        QGroupBox* featuresGroup = new QGroupBox(tr("Terrain Features"), tab);
        QGridLayout* featuresLayout = new QGridLayout(featuresGroup);
        
        mGenerateWater = new QCheckBox(tr("Generate Water"), featuresGroup);
        mGenerateWater->setChecked(true);
        featuresLayout->addWidget(mGenerateWater, 0, 0);
        
        featuresLayout->addWidget(new QLabel(tr("Water Level:"), featuresGroup), 0, 1);
        mWaterLevel = new QDoubleSpinBox(featuresGroup);
        mWaterLevel->setRange(-10000, 10000);
        mWaterLevel->setValue(0);
        featuresLayout->addWidget(mWaterLevel, 0, 2);
        
        featuresLayout->addWidget(new QLabel(tr("Mountain Frequency:"), featuresGroup), 1, 0);
        mMountainFreq = new QSlider(Qt::Horizontal, featuresGroup);
        mMountainFreq->setRange(1, 100);
        mMountainFreq->setValue(10);
        featuresLayout->addWidget(mMountainFreq, 1, 1);
        mMountainFreqLabel = new QLabel("0.10", featuresGroup);
        mMountainFreqLabel->setMinimumWidth(40);
        featuresLayout->addWidget(mMountainFreqLabel, 1, 2);
        connect(mMountainFreq, &QSlider::valueChanged, [this](int v) {
            mMountainFreqLabel->setText(QString::number(v / 100.0, 'f', 2));
        });
        
        featuresLayout->addWidget(new QLabel(tr("Valley Depth:"), featuresGroup), 2, 0);
        mValleyDepth = new QSlider(Qt::Horizontal, featuresGroup);
        mValleyDepth->setRange(0, 100);
        mValleyDepth->setValue(30);
        featuresLayout->addWidget(mValleyDepth, 2, 1);
        mValleyDepthLabel = new QLabel("0.30", featuresGroup);
        mValleyDepthLabel->setMinimumWidth(40);
        featuresLayout->addWidget(mValleyDepthLabel, 2, 2);
        connect(mValleyDepth, &QSlider::valueChanged, [this](int v) {
            mValleyDepthLabel->setText(QString::number(v / 100.0, 'f', 2));
        });
        
        layout->addWidget(featuresGroup);
        
        layout->addStretch();
        
        scrollArea->setWidget(tab);
        scrollArea->setWidgetResizable(true);
        mTabWidget->addTab(scrollArea, tr("Terrain"));
    }
    
    void ProceduralGeneratorDialog::createObjectsTab()
    {
        QScrollArea* scrollArea = new QScrollArea();
        QWidget* tab = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(tab);
        
        // Density Settings
        QGroupBox* densityGroup = new QGroupBox(tr("Object Density"), tab);
        QGridLayout* densityLayout = new QGridLayout(densityGroup);
        
        densityLayout->addWidget(new QLabel(tr("Trees:"), densityGroup), 0, 0);
        mTreeDensity = new QSlider(Qt::Horizontal, densityGroup);
        mTreeDensity->setRange(0, 100);
        mTreeDensity->setValue(30);
        densityLayout->addWidget(mTreeDensity, 0, 1);
        mTreeDensityLabel = new QLabel("0.30", densityGroup);
        mTreeDensityLabel->setMinimumWidth(40);
        densityLayout->addWidget(mTreeDensityLabel, 0, 2);
        connect(mTreeDensity, &QSlider::valueChanged, [this](int v) {
            mTreeDensityLabel->setText(QString::number(v / 100.0, 'f', 2));
        });
        
        densityLayout->addWidget(new QLabel(tr("Rocks:"), densityGroup), 1, 0);
        mRockDensity = new QSlider(Qt::Horizontal, densityGroup);
        mRockDensity->setRange(0, 100);
        mRockDensity->setValue(20);
        densityLayout->addWidget(mRockDensity, 1, 1);
        mRockDensityLabel = new QLabel("0.20", densityGroup);
        mRockDensityLabel->setMinimumWidth(40);
        densityLayout->addWidget(mRockDensityLabel, 1, 2);
        connect(mRockDensity, &QSlider::valueChanged, [this](int v) {
            mRockDensityLabel->setText(QString::number(v / 100.0, 'f', 2));
        });
        
        densityLayout->addWidget(new QLabel(tr("Grass/Flora:"), densityGroup), 2, 0);
        mGrassDensity = new QSlider(Qt::Horizontal, densityGroup);
        mGrassDensity->setRange(0, 100);
        mGrassDensity->setValue(50);
        densityLayout->addWidget(mGrassDensity, 2, 1);
        mGrassDensityLabel = new QLabel("0.50", densityGroup);
        mGrassDensityLabel->setMinimumWidth(40);
        densityLayout->addWidget(mGrassDensityLabel, 2, 2);
        connect(mGrassDensity, &QSlider::valueChanged, [this](int v) {
            mGrassDensityLabel->setText(QString::number(v / 100.0, 'f', 2));
        });
        
        densityLayout->addWidget(new QLabel(tr("Buildings:"), densityGroup), 3, 0);
        mBuildingDensity = new QSlider(Qt::Horizontal, densityGroup);
        mBuildingDensity->setRange(0, 30);
        mBuildingDensity->setValue(5);
        densityLayout->addWidget(mBuildingDensity, 3, 1);
        mBuildingDensityLabel = new QLabel("0.05", densityGroup);
        mBuildingDensityLabel->setMinimumWidth(40);
        densityLayout->addWidget(mBuildingDensityLabel, 3, 2);
        connect(mBuildingDensity, &QSlider::valueChanged, [this](int v) {
            mBuildingDensityLabel->setText(QString::number(v / 100.0, 'f', 2));
        });
        
        layout->addWidget(densityGroup);
        
        // Placement Settings
        QGroupBox* placementGroup = new QGroupBox(tr("Placement Settings"), tab);
        QGridLayout* placementLayout = new QGridLayout(placementGroup);
        
        placementLayout->addWidget(new QLabel(tr("Min Spacing:"), placementGroup), 0, 0);
        mMinSpacing = new QDoubleSpinBox(placementGroup);
        mMinSpacing->setRange(10, 1000);
        mMinSpacing->setValue(100);
        placementLayout->addWidget(mMinSpacing, 0, 1, 1, 2);
        
        placementLayout->addWidget(new QLabel(tr("Clustering:"), placementGroup), 1, 0);
        mClustering = new QSlider(Qt::Horizontal, placementGroup);
        mClustering->setRange(0, 100);
        mClustering->setValue(50);
        placementLayout->addWidget(mClustering, 1, 1);
        mClusteringLabel = new QLabel("0.50", placementGroup);
        mClusteringLabel->setMinimumWidth(40);
        placementLayout->addWidget(mClusteringLabel, 1, 2);
        connect(mClustering, &QSlider::valueChanged, [this](int v) {
            mClusteringLabel->setText(QString::number(v / 100.0, 'f', 2));
        });
        
        mAlignToTerrain = new QCheckBox(tr("Align objects to terrain slope"), placementGroup);
        mAlignToTerrain->setChecked(true);
        placementLayout->addWidget(mAlignToTerrain, 2, 0, 1, 3);
        
        placementLayout->addWidget(new QLabel(tr("Scale Variation:"), placementGroup), 3, 0);
        mScaleVariation = new QSlider(Qt::Horizontal, placementGroup);
        mScaleVariation->setRange(0, 50);
        mScaleVariation->setValue(20);
        placementLayout->addWidget(mScaleVariation, 3, 1);
        mScaleVariationLabel = new QLabel("0.20", placementGroup);
        mScaleVariationLabel->setMinimumWidth(40);
        placementLayout->addWidget(mScaleVariationLabel, 3, 2);
        connect(mScaleVariation, &QSlider::valueChanged, [this](int v) {
            mScaleVariationLabel->setText(QString::number(v / 100.0, 'f', 2));
        });
        
        placementLayout->addWidget(new QLabel(tr("Rotation Variation:"), placementGroup), 4, 0);
        mRotationVariation = new QSlider(Qt::Horizontal, placementGroup);
        mRotationVariation->setRange(0, 100);
        mRotationVariation->setValue(100);
        placementLayout->addWidget(mRotationVariation, 4, 1);
        mRotationVariationLabel = new QLabel("1.00", placementGroup);
        mRotationVariationLabel->setMinimumWidth(40);
        placementLayout->addWidget(mRotationVariationLabel, 4, 2);
        connect(mRotationVariation, &QSlider::valueChanged, [this](int v) {
            mRotationVariationLabel->setText(QString::number(v / 100.0, 'f', 2));
        });
        
        layout->addWidget(placementGroup);
        
        layout->addStretch();
        
        scrollArea->setWidget(tab);
        scrollArea->setWidgetResizable(true);
        mTabWidget->addTab(scrollArea, tr("Objects"));
    }
    
    void ProceduralGeneratorDialog::createInteriorsTab()
    {
        QScrollArea* scrollArea = new QScrollArea();
        QWidget* tab = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(tab);
        
        // Room Settings
        QGroupBox* roomGroup = new QGroupBox(tr("Room Generation"), tab);
        QGridLayout* roomLayout = new QGridLayout(roomGroup);
        
        roomLayout->addWidget(new QLabel(tr("Min Rooms:"), roomGroup), 0, 0);
        mMinRooms = new QSpinBox(roomGroup);
        mMinRooms->setRange(1, 50);
        mMinRooms->setValue(3);
        roomLayout->addWidget(mMinRooms, 0, 1);
        
        roomLayout->addWidget(new QLabel(tr("Max Rooms:"), roomGroup), 0, 2);
        mMaxRooms = new QSpinBox(roomGroup);
        mMaxRooms->setRange(1, 100);
        mMaxRooms->setValue(12);
        roomLayout->addWidget(mMaxRooms, 0, 3);
        
        roomLayout->addWidget(new QLabel(tr("Room Size Min:"), roomGroup), 1, 0);
        mRoomSizeMin = new QDoubleSpinBox(roomGroup);
        mRoomSizeMin->setRange(100, 2000);
        mRoomSizeMin->setValue(200);
        roomLayout->addWidget(mRoomSizeMin, 1, 1);
        
        roomLayout->addWidget(new QLabel(tr("Room Size Max:"), roomGroup), 1, 2);
        mRoomSizeMax = new QDoubleSpinBox(roomGroup);
        mRoomSizeMax->setRange(200, 5000);
        mRoomSizeMax->setValue(800);
        roomLayout->addWidget(mRoomSizeMax, 1, 3);
        
        roomLayout->addWidget(new QLabel(tr("Corridor Width:"), roomGroup), 2, 0);
        mCorridorWidth = new QDoubleSpinBox(roomGroup);
        mCorridorWidth->setRange(50, 500);
        mCorridorWidth->setValue(150);
        roomLayout->addWidget(mCorridorWidth, 2, 1);
        
        roomLayout->addWidget(new QLabel(tr("Ceiling Height:"), roomGroup), 2, 2);
        mCeilingHeight = new QDoubleSpinBox(roomGroup);
        mCeilingHeight->setRange(100, 1000);
        mCeilingHeight->setValue(300);
        roomLayout->addWidget(mCeilingHeight, 2, 3);
        
        layout->addWidget(roomGroup);
        
        // Content Settings
        QGroupBox* contentGroup = new QGroupBox(tr("Interior Content"), tab);
        QVBoxLayout* contentLayout = new QVBoxLayout(contentGroup);
        
        mGenerateLighting = new QCheckBox(tr("Generate lighting (torches, lamps)"), contentGroup);
        mGenerateLighting->setChecked(true);
        contentLayout->addWidget(mGenerateLighting);
        
        mGenerateContainers = new QCheckBox(tr("Generate containers (chests, barrels)"), contentGroup);
        mGenerateContainers->setChecked(true);
        contentLayout->addWidget(mGenerateContainers);
        
        mGenerateNPCs = new QCheckBox(tr("Generate NPCs (experimental)"), contentGroup);
        mGenerateNPCs->setChecked(false);
        contentLayout->addWidget(mGenerateNPCs);
        
        QHBoxLayout* clutterLayout = new QHBoxLayout();
        clutterLayout->addWidget(new QLabel(tr("Clutter Amount:"), contentGroup));
        mClutter = new QSlider(Qt::Horizontal, contentGroup);
        mClutter->setRange(0, 100);
        mClutter->setValue(40);
        clutterLayout->addWidget(mClutter);
        mClutterLabel = new QLabel("0.40", contentGroup);
        mClutterLabel->setMinimumWidth(40);
        clutterLayout->addWidget(mClutterLabel);
        connect(mClutter, &QSlider::valueChanged, [this](int v) {
            mClutterLabel->setText(QString::number(v / 100.0, 'f', 2));
        });
        contentLayout->addLayout(clutterLayout);
        
        layout->addWidget(contentGroup);
        
        layout->addStretch();
        
        scrollArea->setWidget(tab);
        scrollArea->setWidgetResizable(true);
        mTabWidget->addTab(scrollArea, tr("Interiors"));
    }
    
    void ProceduralGeneratorDialog::createAdvancedTab()
    {
        QWidget* tab = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(tab);
        
        QLabel* analysisLabel = new QLabel(tr("Analysis Results:"), tab);
        layout->addWidget(analysisLabel);
        
        mAnalysisResults = new QTextEdit(tab);
        mAnalysisResults->setReadOnly(true);
        mAnalysisResults->setPlaceholderText(
            tr("Analysis results will appear here after analyzing a reference worldspace.\n\n"
               "This shows:\n"
               "- Terrain height statistics\n"
               "- Texture distribution\n"
               "- Object placement patterns\n"
               "- Derived generation parameters"));
        layout->addWidget(mAnalysisResults);
        
        mTabWidget->addTab(tab, tr("Advanced"));
    }
    
    void ProceduralGeneratorDialog::loadStateToUi()
    {
        const auto& state = mGenerator->getState();
        
        // General
        mWorldSizeX->setValue(state.worldSizeX);
        mWorldSizeY->setValue(state.worldSizeY);
        mOriginX->setValue(state.originX);
        mOriginY->setValue(state.originY);
        mSeedEdit->setText(state.seed > 0 ? QString::number(state.seed) : "");
        mUseReference->setChecked(state.useReference);
        mReferenceWorldspace->setText(QString::fromStdString(state.referenceWorldspace));
        mGenerateExteriors->setChecked(state.generateExteriors);
        mGenerateInteriors->setChecked(state.generateInteriors);
        mGeneratePathgrids->setChecked(state.generatePathgrids);
        mOverwriteExisting->setChecked(state.overwriteExisting);
        
        // Terrain
        mBaseHeight->setValue(state.terrain.baseHeight);
        mHeightVariation->setValue(state.terrain.heightVariation);
        mRoughness->setValue(static_cast<int>(state.terrain.roughness * 100));
        mErosion->setValue(static_cast<int>(state.terrain.erosionStrength * 100));
        mOctaves->setValue(state.terrain.octaves);
        mPersistence->setValue(state.terrain.persistence);
        mLacunarity->setValue(state.terrain.lacunarity);
        mGenerateWater->setChecked(state.terrain.generateWater);
        mWaterLevel->setValue(state.terrain.waterLevel);
        mMountainFreq->setValue(static_cast<int>(state.terrain.mountainFrequency * 100));
        mValleyDepth->setValue(static_cast<int>(state.terrain.valleyDepth * 100));
        
        // Objects
        mTreeDensity->setValue(static_cast<int>(state.objects.treeDensity * 100));
        mRockDensity->setValue(static_cast<int>(state.objects.rockDensity * 100));
        mGrassDensity->setValue(static_cast<int>(state.objects.grassDensity * 100));
        mBuildingDensity->setValue(static_cast<int>(state.objects.buildingDensity * 100));
        mMinSpacing->setValue(state.objects.minSpacing);
        mClustering->setValue(static_cast<int>(state.objects.clusteringFactor * 100));
        mAlignToTerrain->setChecked(state.objects.alignToTerrain);
        mScaleVariation->setValue(static_cast<int>(state.objects.scaleVariation * 100));
        mRotationVariation->setValue(static_cast<int>(state.objects.rotationVariation * 100));
        
        // Interiors
        mMinRooms->setValue(state.interiors.minRooms);
        mMaxRooms->setValue(state.interiors.maxRooms);
        mRoomSizeMin->setValue(state.interiors.roomSizeMin);
        mRoomSizeMax->setValue(state.interiors.roomSizeMax);
        mCorridorWidth->setValue(state.interiors.corridorWidth);
        mCeilingHeight->setValue(state.interiors.ceilingHeight);
        mGenerateLighting->setChecked(state.interiors.generateLighting);
        mGenerateContainers->setChecked(state.interiors.generateContainers);
        mGenerateNPCs->setChecked(state.interiors.generateNPCs);
        mClutter->setValue(static_cast<int>(state.interiors.clutter * 100));
    }
    
    void ProceduralGeneratorDialog::saveUiToState()
    {
        auto& state = mGenerator->getState();
        
        // General
        state.worldSizeX = mWorldSizeX->value();
        state.worldSizeY = mWorldSizeY->value();
        state.originX = mOriginX->value();
        state.originY = mOriginY->value();
        
        QString seedText = mSeedEdit->text().trimmed();
        if (seedText.isEmpty())
            state.seed = 0;
        else
            state.seed = seedText.toULongLong();
            
        state.useReference = mUseReference->isChecked();
        state.referenceWorldspace = mReferenceWorldspace->text().toStdString();
        state.generateExteriors = mGenerateExteriors->isChecked();
        state.generateInteriors = mGenerateInteriors->isChecked();
        state.generatePathgrids = mGeneratePathgrids->isChecked();
        state.overwriteExisting = mOverwriteExisting->isChecked();
        
        // Terrain
        state.terrain.baseHeight = static_cast<float>(mBaseHeight->value());
        state.terrain.heightVariation = static_cast<float>(mHeightVariation->value());
        state.terrain.roughness = mRoughness->value() / 100.0f;
        state.terrain.erosionStrength = mErosion->value() / 100.0f;
        state.terrain.octaves = mOctaves->value();
        state.terrain.persistence = static_cast<float>(mPersistence->value());
        state.terrain.lacunarity = static_cast<float>(mLacunarity->value());
        state.terrain.generateWater = mGenerateWater->isChecked();
        state.terrain.waterLevel = static_cast<float>(mWaterLevel->value());
        state.terrain.mountainFrequency = mMountainFreq->value() / 100.0f;
        state.terrain.valleyDepth = mValleyDepth->value() / 100.0f;
        
        // Objects
        state.objects.treeDensity = mTreeDensity->value() / 100.0f;
        state.objects.rockDensity = mRockDensity->value() / 100.0f;
        state.objects.grassDensity = mGrassDensity->value() / 100.0f;
        state.objects.buildingDensity = mBuildingDensity->value() / 100.0f;
        state.objects.minSpacing = static_cast<float>(mMinSpacing->value());
        state.objects.clusteringFactor = mClustering->value() / 100.0f;
        state.objects.alignToTerrain = mAlignToTerrain->isChecked();
        state.objects.scaleVariation = mScaleVariation->value() / 100.0f;
        state.objects.rotationVariation = mRotationVariation->value() / 100.0f;
        
        // Interiors
        state.interiors.minRooms = mMinRooms->value();
        state.interiors.maxRooms = mMaxRooms->value();
        state.interiors.roomSizeMin = static_cast<float>(mRoomSizeMin->value());
        state.interiors.roomSizeMax = static_cast<float>(mRoomSizeMax->value());
        state.interiors.corridorWidth = static_cast<float>(mCorridorWidth->value());
        state.interiors.ceilingHeight = static_cast<float>(mCeilingHeight->value());
        state.interiors.generateLighting = mGenerateLighting->isChecked();
        state.interiors.generateContainers = mGenerateContainers->isChecked();
        state.interiors.generateNPCs = mGenerateNPCs->isChecked();
        state.interiors.clutter = mClutter->value() / 100.0f;
    }
    
    void ProceduralGeneratorDialog::setControlsEnabled(bool enabled)
    {
        mTabWidget->setEnabled(enabled);
        mGenerateButton->setEnabled(enabled);
        mPreviewButton->setEnabled(enabled);
        mCancelButton->setEnabled(!enabled);
    }
    
    void ProceduralGeneratorDialog::onGenerate()
    {
        saveUiToState();
        setControlsEnabled(false);
        
        mProgressBar->setValue(0);
        mStatusLabel->setText(tr("Starting generation..."));
        
        // Run generation (in a real implementation, this would be in a separate thread)
        bool success = mGenerator->generate();
        
        setControlsEnabled(true);
        
        if (success)
        {
            mProgressBar->setValue(100);
            mStatusLabel->setText(tr("Generation complete!"));
            QMessageBox::information(this, tr("Success"), 
                tr("Procedural world generation completed successfully.\n\n"
                   "The generated content has been added to your document.\n"
                   "Remember to save your work!"));
        }
        else if (mGenerator->isRunning())
        {
            mStatusLabel->setText(tr("Generation cancelled."));
        }
        else
        {
            mStatusLabel->setText(tr("Generation failed."));
        }
    }
    
    void ProceduralGeneratorDialog::onCancel()
    {
        mGenerator->cancel();
        mStatusLabel->setText(tr("Cancelling..."));
    }
    
    void ProceduralGeneratorDialog::onAnalyzeReference()
    {
        saveUiToState();
        
        mStatusLabel->setText(tr("Analyzing reference worldspace..."));
        mProgressBar->setValue(0);
        
        CSMProcs::WorldAnalyzer analyzer(mDocument.getData());
        auto results = analyzer.analyzeWorldspace(mReferenceWorldspace->text().toStdString());
        
        if (results.isValid)
        {
            // Store results
            mGenerator->getState().analysis = results;
            
            // Apply derived parameters
            auto terrainParams = analyzer.deriveTerrainParams(results);
            auto objectParams = analyzer.deriveObjectParams(results);
            
            mGenerator->getState().terrain = terrainParams;
            mGenerator->getState().objects = objectParams;
            
            // Update UI
            loadStateToUi();
            
            // Show results
            QString resultText;
            resultText += tr("=== Terrain Analysis ===\n");
            resultText += tr("Average Height: %1\n").arg(results.avgHeight, 0, 'f', 1);
            resultText += tr("Height Std Dev: %1\n").arg(results.heightStdDev, 0, 'f', 1);
            resultText += tr("Min Height: %1\n").arg(results.minHeight, 0, 'f', 1);
            resultText += tr("Max Height: %1\n").arg(results.maxHeight, 0, 'f', 1);
            resultText += tr("Average Roughness: %1\n").arg(results.avgRoughness, 0, 'f', 3);
            
            resultText += tr("\n=== Texture Distribution ===\n");
            for (const auto& [tex, freq] : results.textureFrequency)
            {
                resultText += tr("%1: %2%\n").arg(QString::fromStdString(tex)).arg(freq * 100, 0, 'f', 1);
            }
            
            resultText += tr("\n=== Object Density ===\n");
            for (const auto& [type, density] : results.objectDensityByType)
            {
                resultText += tr("%1: %2 per million sq units\n")
                    .arg(QString::fromStdString(type)).arg(density, 0, 'f', 2);
            }
            
            resultText += tr("\nAverage Object Spacing: %1 units\n").arg(results.avgObjectSpacing, 0, 'f', 1);
            
            mAnalysisResults->setText(resultText);
            mStatusLabel->setText(tr("Analysis complete - parameters updated."));
            mProgressBar->setValue(100);
        }
        else
        {
            mStatusLabel->setText(tr("Analysis failed - no valid data found."));
            QMessageBox::warning(this, tr("Analysis Failed"),
                tr("Could not analyze the reference worldspace.\n\n"
                   "Make sure you have loaded content files that contain exterior cells."));
        }
    }
    
    void ProceduralGeneratorDialog::onPreview()
    {
        saveUiToState();
        
        mStatusLabel->setText(tr("Generating preview cell..."));
        
        int previewX = mOriginX->value();
        int previewY = mOriginY->value();
        
        bool success = mGenerator->previewCell(previewX, previewY);
        
        if (success)
        {
            mStatusLabel->setText(tr("Preview cell generated at (%1, %2)").arg(previewX).arg(previewY));
            QMessageBox::information(this, tr("Preview Generated"),
                tr("A preview cell has been generated at coordinates (%1, %2).\n\n"
                   "You can view it in the 3D scene view.").arg(previewX).arg(previewY));
        }
        else
        {
            mStatusLabel->setText(tr("Preview generation failed."));
        }
    }
    
    void ProceduralGeneratorDialog::onRandomSeed()
    {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint64_t> dis;
        
        mSeedEdit->setText(QString::number(dis(gen)));
    }
    
    void ProceduralGeneratorDialog::onReferenceChanged(const QString& text)
    {
        mAnalyzeButton->setEnabled(!text.isEmpty() && mUseReference->isChecked());
    }
    
    void ProceduralGeneratorDialog::onUseReferenceChanged(int state)
    {
        bool enabled = state == Qt::Checked;
        mReferenceWorldspace->setEnabled(enabled);
        mAnalyzeButton->setEnabled(enabled && !mReferenceWorldspace->text().isEmpty());
    }
    
    void ProceduralGeneratorDialog::onProgress(int current, int total, const QString& message)
    {
        if (total > 0)
        {
            mProgressBar->setValue(current * 100 / total);
        }
        mStatusLabel->setText(message);
    }
}
