// Display-setting controls -- the C++ analog of dd_molview's
// `desktop/controls.py::DisplaySettingsPanel`. Same controls, same
// defaults.
#pragma once

#include <QWidget>

#include "PythonBridge.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;

class DisplaySettingsPanel : public QWidget {
    Q_OBJECT
public:
    explicit DisplaySettingsPanel(QWidget* parent = nullptr);

    DisplaySettings currentSettings() const;
    double contactCutoff() const;

    // Enables/disables "Show reference ligand" (and unchecks it when
    // disabled) -- called whenever the loaded reference ligand changes.
    void setReferenceAvailable(bool available);

signals:
    void settingsChanged();
    void centerOnLigandRequested();
    // "Zoom to Highlighted Residues": re-fit the camera to whatever
    // residues are currently rendered yellow (the interaction-detected
    // set -- see DisplaySettings::showInteractingResidues), the same way
    // centerOnLigandRequested re-fits it to the ligand. MainWindow owns
    // the actual residue list (it comes back from PythonBridge::buildView's
    // ViewResult, not from anything this panel tracks), so this signal
    // carries no payload -- MainWindow reads its own last-known set.
    void zoomToHighlightedResiduesRequested();

private:
    void buildLayout();
    void wireSignals();
    void updateCutoffLabel(int sliderValue);

    QPushButton* centerOnLigandButton_;
    QPushButton* zoomToHighlightedButton_;
    QComboBox* receptorStyleCombo_;
    QCheckBox* colorBySSCheck_;
    QCheckBox* onlyNearLigandCheck_;
    QCheckBox* showHbondsCheck_;
    QCheckBox* showHydrophobicCheck_;
    QCheckBox* showSaltBridgesCheck_;
    QCheckBox* showElectrostaticCheck_;
    QCheckBox* showPiStackingCheck_;
    QCheckBox* showPiHalogenCheck_;
    QCheckBox* showSulfurHalogenCheck_;
    QCheckBox* showInteractingResiduesCheck_;
    QSlider* contactCutoffSlider_;
    QLabel* contactCutoffLabel_;
    QCheckBox* showReferenceCheck_;
};
