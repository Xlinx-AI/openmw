#ifndef CSV_PROCS_PROCEDURALGENERATORDIALOG_H
#define CSV_PROCS_PROCEDURALGENERATORDIALOG_H

#include <memory>

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QSlider;
class QSpinBox;
class QTabWidget;
class QTextEdit;

namespace CSMDoc
{
    class Document;
}

namespace CSMProcs
{
    class ProceduralGenerator;
    struct ProceduralState;
}

namespace CSVProcs
{
    /// Dialog for configuring and running procedural world generation
    class ProceduralGeneratorDialog : public QDialog
    {
        Q_OBJECT

    public:
        ProceduralGeneratorDialog(CSMDoc::Document& document, QWidget* parent = nullptr);
        ~ProceduralGeneratorDialog() override;

    private slots:
        void onGenerate();
        void onCancel();
        void onAnalyzeReference();
        void onPreview();
        void onRandomSeed();
        void onReferenceChanged(const QString& text);
        void onUseReferenceChanged(int state);
        void onProgress(int current, int total, const QString& message);
        
    private:
        void setupUi();
        void createGeneralTab();
        void createTerrainTab();
        void createObjectsTab();
        void createInteriorsTab();
        void createAdvancedTab();
        
        void loadStateToUi();
        void saveUiToState();
        
        void setControlsEnabled(bool enabled);
        
        CSMDoc::Document& mDocument;
        std::unique_ptr<CSMProcs::ProceduralGenerator> mGenerator;
        
        // Main UI
        QTabWidget* mTabWidget = nullptr;
        QProgressBar* mProgressBar = nullptr;
        QLabel* mStatusLabel = nullptr;
        QPushButton* mGenerateButton = nullptr;
        QPushButton* mCancelButton = nullptr;
        QPushButton* mPreviewButton = nullptr;
        
        // General tab
        QSpinBox* mWorldSizeX = nullptr;
        QSpinBox* mWorldSizeY = nullptr;
        QSpinBox* mOriginX = nullptr;
        QSpinBox* mOriginY = nullptr;
        QLineEdit* mSeedEdit = nullptr;
        QPushButton* mRandomSeedButton = nullptr;
        QCheckBox* mUseReference = nullptr;
        QLineEdit* mReferenceWorldspace = nullptr;
        QPushButton* mAnalyzeButton = nullptr;
        QCheckBox* mGenerateExteriors = nullptr;
        QCheckBox* mGenerateInteriors = nullptr;
        QCheckBox* mGeneratePathgrids = nullptr;
        QCheckBox* mOverwriteExisting = nullptr;
        
        // Terrain tab
        QDoubleSpinBox* mBaseHeight = nullptr;
        QDoubleSpinBox* mHeightVariation = nullptr;
        QSlider* mRoughness = nullptr;
        QLabel* mRoughnessLabel = nullptr;
        QSlider* mErosion = nullptr;
        QLabel* mErosionLabel = nullptr;
        QSpinBox* mOctaves = nullptr;
        QDoubleSpinBox* mPersistence = nullptr;
        QDoubleSpinBox* mLacunarity = nullptr;
        QCheckBox* mGenerateWater = nullptr;
        QDoubleSpinBox* mWaterLevel = nullptr;
        QSlider* mMountainFreq = nullptr;
        QLabel* mMountainFreqLabel = nullptr;
        QSlider* mValleyDepth = nullptr;
        QLabel* mValleyDepthLabel = nullptr;
        
        // Objects tab
        QSlider* mTreeDensity = nullptr;
        QLabel* mTreeDensityLabel = nullptr;
        QSlider* mRockDensity = nullptr;
        QLabel* mRockDensityLabel = nullptr;
        QSlider* mGrassDensity = nullptr;
        QLabel* mGrassDensityLabel = nullptr;
        QSlider* mBuildingDensity = nullptr;
        QLabel* mBuildingDensityLabel = nullptr;
        QDoubleSpinBox* mMinSpacing = nullptr;
        QSlider* mClustering = nullptr;
        QLabel* mClusteringLabel = nullptr;
        QCheckBox* mAlignToTerrain = nullptr;
        QSlider* mScaleVariation = nullptr;
        QLabel* mScaleVariationLabel = nullptr;
        QSlider* mRotationVariation = nullptr;
        QLabel* mRotationVariationLabel = nullptr;
        
        // Interiors tab
        QSpinBox* mMinRooms = nullptr;
        QSpinBox* mMaxRooms = nullptr;
        QDoubleSpinBox* mRoomSizeMin = nullptr;
        QDoubleSpinBox* mRoomSizeMax = nullptr;
        QDoubleSpinBox* mCorridorWidth = nullptr;
        QDoubleSpinBox* mCeilingHeight = nullptr;
        QCheckBox* mGenerateLighting = nullptr;
        QCheckBox* mGenerateContainers = nullptr;
        QCheckBox* mGenerateNPCs = nullptr;
        QSlider* mClutter = nullptr;
        QLabel* mClutterLabel = nullptr;
        
        // Advanced tab
        QTextEdit* mAnalysisResults = nullptr;
    };
}

#endif
