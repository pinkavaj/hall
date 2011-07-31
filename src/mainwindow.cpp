#include <errno.h>
#include <math.h>
#include <QCloseEvent>
#include <QDateTime>
#include <QMessageBox>
#include <stdexcept>
#include <vector>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#endif

#include "mainwindow.h"
#include "ui_mainwindow.h"

const char MainWindow::pol_pn[] =
        "<span style='font-weight:600;'><span style='color:#ff0000;'>+</span> <span style='color:#0000ff;'>-</span></span>";
const char MainWindow::pol_np[] =
        "<span style='font-weight:600;'><span style='color:#0000ff;'>-</span> <span style='color:#ff0000;'>+</span></span>";

const int MainWindow::_34901A = 100;
const int MainWindow::_34901A_sample_cd = _34901A + 1;
const int MainWindow::_34901A_sample_da = _34901A + 2;
const int MainWindow::_34901A_sample_bd = _34901A + 3;
const int MainWindow::_34901A_sample_ac = _34901A + 4;
const int MainWindow::_34901A_hall_probe = _34901A + 14;

const int MainWindow::_34903A = 200;
const int MainWindow::_34903A_sample_a_pwr_m = _34903A + 1;
const int MainWindow::_34903A_sample_b_pwr_p = _34903A + 2;
const int MainWindow::_34903A_sample_c_pwr_sw1 = _34903A + 3;
const int MainWindow::_34903A_sample_d_pwr_m = _34903A + 4;
const int MainWindow::_34903A_pwr_sw1_pwr_m = _34903A + 5;
const int MainWindow::_34903A_pwr_sw1_pwr_p = _34903A + 6;
const int MainWindow::_34903A_hall_probe_1_pwr_m = _34903A + 9;
const int MainWindow::_34903A_hall_probe_2_pwr_p = _34903A + 10;

const MainWindow::Step_t MainWindow::stepsAll[] = {
    {   stepAbort, 0,    },
};

const MainWindow::Step_t MainWindow::stepsMeasure[] = {
    {   stepOpenAllRoutes, 100, },
    {   stepGetTime, 0 },
    {   stepMeasHallProbePrepare, 100 },
    {   stepMeasHallProbe, 0 },
    {   stepMeasHallProbeFinish, 0 },
    {   stepFinish, 0 },
    {   stepAbort, 0 },
};

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    measRunning(false),
    configUI(),
    ui(new Ui::MainWindow)
{

    ui->setupUi(this);

    QObject::connect(&configUI, SIGNAL(accepted()), this, SLOT(show()));

    coilTimer.setInterval(currentDwell);
    coilTimer.setSingleShot(false);
    QObject::connect(&coilTimer, SIGNAL(timeout()), this,
                     SLOT(on_currentTimer_timeout()));

    measTimer.setSingleShot(true);
    QObject::connect(&measTimer, SIGNAL(timeout()), this,
                     SLOT(on_measTimer_timeout()));
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::stepAbort(MainWindow *)
{
    return false;
}

bool MainWindow::stepCreateLoopMark(MainWindow *this_)
{
    this_->stepLoopMark = this_->stepCurrent + 1;

    return true;
}

bool MainWindow::stepFinish(MainWindow *this_)
{
    this_->csvFile.write();

    return true;
}

bool MainWindow::stepGetTime(MainWindow *this_)
{
    QString s;

    s = this_->csvFile.setAt(csvColTime, QDateTime::currentDateTimeUtc());
    this_->ui->dataTableWidget->setItem(0, 3, new QTableWidgetItem(s));

    return true;
}

bool MainWindow::stepOpenAllRoutes(MainWindow *this_)
{
    HP34970hack::Channels_t closeChannels;

    this_->hp34970Hack.setRoute(closeChannels, _34903A);

    return true;
}

bool MainWindow::stepMeasHallProbe(MainWindow *this_)
{
    QStringList data;
    bool ok;
    double val;

    data = this_->hp34970Hack.read();

    if (data.size() == 1) {
        val = QVariant(data[0]).toDouble(&ok);
        this_->csvFile.setAt(csvColHallProbeU, val);
        val = -30.588 + sqrt(934.773 + 392.163 * val);
        this_->ui->coilBDoubleSpinBox->setValue(val);
        this_->csvFile.setAt(csvColHallProbeB, val);
    }
    else {
        throw new std::runtime_error("Could not get B data.");
    }

    return true;
}

bool MainWindow::stepMeasHallProbeFinish(MainWindow *this_)
{
    this_->ps622Hack.setOutput(false);

    HP34970hack::Channels_t closeChannels;
    this_->hp34970Hack.setRoute(closeChannels, _34903A);

    return true;
}

bool MainWindow::stepMeasHallProbePrepare(MainWindow *this_)
{
    const double hallProbeI = 0.001;
    /* set current to 1mA, open probe current source */
    this_->ps622Hack.setCurrent(hallProbeI);
    this_->csvFile.setAt(csvColHallProbeI, hallProbeI);

    HP34970hack::Channels_t closeChannels;
    closeChannels.append(_34903A_hall_probe_1_pwr_m);
    closeChannels.append(_34903A_hall_probe_2_pwr_p);
    this_->hp34970Hack.setRoute(closeChannels, _34903A);

    HP34970hack::Channels_t scan;
    scan.append(MainWindow::_34901A_hall_probe);
    this_->hp34970Hack.setScan(scan);
    this_->hp34970Hack.init();

    this_->ps622Hack.setOutput(true);

    return true;
}

void MainWindow::on_measTimer_timeout()
{
    if (stepCurrent != stepsRunning.end()) {
        if (stepCurrent->func(this)) {
            measTimer.start(stepCurrent->delay);
            ++stepCurrent;
            return;
        }
    }
    ui->coilGroupBox->setEnabled(true);
    ui->sampleGroupBox->setEnabled(true);
    measRunning = false;
}

void MainWindow::closeDevs()
{
    coilTimer.stop();
    ui->sweepingWidget->setEnabled(false);
    csvFile.close();
    hp34970Hack.close();
    ps622Hack.close();
    pwrPolSwitch.close();
    sdp_close(&sdp);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (configUI.isHidden() && configUI.result() == QDialog::Accepted) {
        event->ignore();
        if (ui->coilPowerCheckBox->isChecked() ||
                ui->coilPolCrossCheckBox->isChecked() ||
                ui->samplePowerCheckBox->isChecked()) {
            if (QMessageBox::warning(
                        this, "Power is still on!",
                        "Power is still on and should be turned (slowly!) "
                        "off before end of experiment.\n\n"
                        "Exit experiment withought shutdown?",
                        QMessageBox::Yes, QMessageBox::No) != QMessageBox::Yes) {
                // TODO: Offer shut down.
                return;
            }
        }
        closeDevs();
        hide();
        configUI.show();

        return;
    }

    QMainWindow::closeEvent(event);
}

void MainWindow::on_coilCurrDoubleSpinBox_valueChanged(double )
{
    ui->sweepingWidget->setEnabled(true);
}

void MainWindow::on_coilPolCrossCheckBox_toggled(bool checked)
{
    ui->sweepingWidget->setEnabled(true);

    if (checked)
        ui->polarityLabel->setText(pol_np);
    else
        ui->polarityLabel->setText(pol_pn);
}

void MainWindow::on_coilPowerCheckBox_toggled(bool)
{
    ui->sweepingWidget->setEnabled(true);
}

void MainWindow::on_currentTimer_timeout()
{
    sdp_lcd_info_t lcd_info;

    if (sdp_get_lcd_info(&sdp, &lcd_info) < 0) {
        throw new std::runtime_error(
                "on_currentTimer_timeout - sdp_get_lcd_info");
        return;
    }

    ui->coilCurrMeasDoubleSpinBox->setValue(lcd_info.read_A);
    ui->coilVoltMeasDoubleSpinBox->setValue(lcd_info.read_V);

    // update coil current 
    if (!ui->sweepingWidget->isEnabled())
        return;

    /** Curent trought coil */
    double procI, wantI;
    /** Coil power state, on/off */
    bool procCoilPower, wantCoilPower;
    /** Coil power switch state direct/cross */
    PwrPolSwitch::state_t procCoilSwitchState, wantCoilSwitchState;

    /* Get all values necesary for process decisions. */
    // wanted values
    if (ui->coilPolCrossCheckBox->isChecked())
        wantCoilSwitchState = PwrPolSwitch::cross;
    else
        wantCoilSwitchState = PwrPolSwitch::direct;

    wantCoilPower = ui->coilPowerCheckBox->isChecked();

    if (wantCoilPower) {
        wantI = ui->coilCurrDoubleSpinBox->value();
        if (wantCoilSwitchState == PwrPolSwitch::cross)
            wantI = -wantI;
    }
    else
        wantI = 0;

    // process values
    procCoilSwitchState = pwrPolSwitch.polarity();

    procCoilPower = lcd_info.output;
    procI = lcd_info.set_A;
    if (procCoilSwitchState == PwrPolSwitch::cross)
        procI = -procI;

    ui->plainTextEdit->appendPlainText(QString(
                "procI: %1, procCoilSwitchState: %2, procCoilPower: %3")
            .arg(procI).arg(procCoilSwitchState).arg(procCoilPower));
    ui->plainTextEdit->appendPlainText(QString(
                "wantI: %1, wantCoilSwitchState: %2, wantCoilPower: %3\n")
            .arg(wantI).arg(wantCoilSwitchState).arg(wantCoilPower));

    /* Make process decision. */
    // Need switch polarity?
    if (procCoilSwitchState != wantCoilSwitchState) {
        // Is polarity switch posible? (power is off)
        if (!procCoilPower) {
            pwrPolSwitch.setPolarity(wantCoilSwitchState);
            return;
        }

        // Is posible power-off in order to swich polarity?
        if (fabs(procI) < currentSlope) {
            sdp_set_output(&sdp, 0);
            return;
        }

        // set current near to zero before polarity switching
    }

    // Target reach, finish job
    if (fabs(procI - wantI) < currentSlope) {
        ui->sweepingWidget->setEnabled(false);
        if (!wantCoilPower && fabs(procI) <= currentSlope && procCoilPower) {
            if (sdp_set_output(&sdp, 0) < 0)
                throw new std::runtime_error("timer - sdp_set_output");
        }

        return;
    }

    // want current but power is off -> set power on at current 0.0 A
    if (procCoilPower != wantCoilPower && wantCoilPower) {
        sdp_set_curr(&sdp, 0.0);
        sdp_set_output(&sdp, 1);
        return;
    }

    // power is on, but current neet to be adjusted, do one step
    if (procI > wantI)
        procI -= currentSlope;
    else
        procI += currentSlope;

    sdp_set_curr(&sdp, fabs(procI));
}

MainWindow::Steps_t::Steps_t(const Step_t *begin, const Step_t *end)
{
    reserve(end - begin);
    for (; begin < end; ++begin) {
        append(*begin);
    }
}

void MainWindow::on_measurePushButton_clicked()
{
    ui->dataTableWidget->insertRow(0);

    stepsRunning = Steps_t(
                stepsMeasure,
                stepsMeasure + ARRAY_SIZE(stepsMeasure));
    stepCurrent = stepsRunning.begin();

    measTimer.start(0);
    measRunning = true;
    ui->coilGroupBox->setEnabled(false);
    ui->sampleGroupBox->setEnabled(false);

    return;

    QString s;
    double val;

    val = ui->coilCurrMeasDoubleSpinBox->value();
    s = csvFile.setAt(1, val);
    ui->dataTableWidget->setItem(0, 0, new QTableWidgetItem(s));

    val = ui->coilVoltMeasDoubleSpinBox->value();
    csvFile.setAt(2, val);

    val = ui->coilCurrDoubleSpinBox->value();
    csvFile.setAt(3, val);

    val = ui->sampleCurrDoubleSpinBox->value();
    s = csvFile.setAt(4, val);
    ui->dataTableWidget->setItem(0, 1, new QTableWidgetItem(s));
}

void MainWindow::on_sampleCurrDoubleSpinBox_valueChanged(double value)
{
    ps622Hack.setCurrent(value);
}

void MainWindow::on_samplePolCrossCheckBox_toggled(bool )
{

}

void MainWindow::on_samplePowerCheckBox_toggled(bool checked)
{
    ps622Hack.setOutput(checked);
}

bool MainWindow::openDevs()
{
    /** Text and title shown in error message box */
    QString err_text, err_title;
    QString s;
    int err;

    s = settings.value(ConfigUI::cfg_powerSupplyPort).toString();
    err = sdp_open(&sdp, s.toLocal8Bit().constData(), SDP_DEV_ADDR_MIN);
    if (err < 0)
        goto sdp_err0;

    /* Set value limit in current input spin box. */
    sdp_va_t limits;
    err = sdp_get_va_maximums(&sdp, &limits);
    if (err < 0)
        goto sdp_err;
    ui->coilCurrDoubleSpinBox->setMaximum(limits.curr);

    /* Set actual current value as wanted value, avoiding unwanted hickups. */
    sdp_va_data_t va_data;
    err = sdp_get_va_data(&sdp, &va_data);
    if (err < 0)
        goto sdp_err;

    ui->coilCurrDoubleSpinBox->setValue(va_data.curr);
    err = sdp_set_curr(&sdp, va_data.curr);
    if (err < 0)
        goto sdp_err;

    /* Set voltage to maximum, we drive only current. */
    err = sdp_set_volt_limit(&sdp, limits.volt);
    if (err < 0)
        goto sdp_err;

    err = sdp_set_volt(&sdp, limits.volt);
    if (err < 0)
        goto sdp_err;

    sdp_lcd_info_t lcd_info;
    sdp_get_lcd_info(&sdp, &lcd_info); // TODO check
    if (err < 0)
        goto sdp_err;
    ui->coilPowerCheckBox->setChecked(lcd_info.output);

    // Open polarity switch device
    s = settings.value(ConfigUI::cfg_polSwitchPort).toString();
    if (!pwrPolSwitch.open(s.toLocal8Bit().constData())) {
        err = errno;
        goto mag_pwr_switch_err;
    }

    bool cross;
    cross = (pwrPolSwitch.polarity() == PwrPolSwitch::cross);
    ui->coilPolCrossCheckBox->setChecked(cross);

    // Open sample power source
    s = settings.value(ConfigUI::cfg_samplePSPort).toString();
    if (!ps622Hack.open(s.toLocal8Bit().constData()))
    {
        err = errno;
        goto sample_pwr_err;
    }

    ui->sampleCurrDoubleSpinBox->setValue(ps622Hack.current());
    ui->samplePowerCheckBox->setChecked(ps622Hack.output());

    // Open CSV file to save data
    s = settings.value(ConfigUI::cfg_fileName).toString();
    csvFile.setFileName(s);
    if (!csvFile.open())
        goto file_err;

    csvFile.resize(csvColEnd);
    csvFile[csvColTime] = "Time (UTC)";
    csvFile[csvColHallProbeI] = "Hall proble I [A]";
    csvFile[csvColHallProbeU] = "Hall proble U [V]";
    csvFile[csvColHallProbeB] = "Hall proble B [T]";
    csvFile[csvColSampleI] = "sample I [A]";
    csvFile[csvColSampleI] = "sample U [V]";
    csvFile[csvColSampleI] = "sample U [V]";
    csvFile[csvColSampleI] = "sample U [V]";
    csvFile[csvColSampleI] = "sample U [V]";
    csvFile[csvColSampleI] = "sample U [V]";
    csvFile[csvColSampleI] = "sample U [V]";
    csvFile[csvColSampleI] = "sample U [V]";
    csvFile[csvColSampleI] = "sample U [V]";
    csvFile.write();

    // Open and setup HP34970 device
    s = settings.value(ConfigUI::cfg_agilentPort).toString();
    if (!hp34970Hack.open(s)) {
        err = errno;
        goto hp34970hack_err;
    }
    hp34970Hack.setup();

    ui->sweepingWidget->setEnabled(true);
    coilTimer.start();

    return true;

    // hp34970Hack.close();

hp34970hack_err:
    if (err_title.isEmpty()) {
        err_title = QString::fromLocal8Bit(
                    "Failed to open HP34970 device");
        err_text = QString::fromLocal8Bit(strerror(err));
    }

file_err:
    ps622Hack.close();
    if (err_title.isEmpty()) {
        err_title = QString::fromLocal8Bit(
                    "Failed to open output file");
        err_text = csvFile.errorString();
    }

sample_pwr_err:
    pwrPolSwitch.close();
    if (err_title.isEmpty()) {
        err_title = QString::fromLocal8Bit(
                    "Failed to open sample power supply (Keithaly 6220)");
        err_text = QString::fromLocal8Bit(strerror(err));
    }

mag_pwr_switch_err:
    if (err_title.isEmpty()) {
        err_title = QString::fromLocal8Bit(
                    "Failed to open power supply switch");
        err_text = QString::fromLocal8Bit(strerror(err));
    }

sdp_err:
    sdp_close(&sdp);

sdp_err0:
    if (err_title.isEmpty()) {
        err_title = QString::fromLocal8Bit(
                    "Failed to open Manson SDP power supply");
        err_text = QString::fromLocal8Bit(sdp_strerror(err));
    }

    err_text = QString("%1:\n\n%2").arg(err_title).arg(err_text);
    QMessageBox::critical(this, err_title, err_text);
    statusBar()->showMessage(err_title);

    return false;
}

void MainWindow::show()
{
    if (!openDevs()) {
        configUI.show();

        return;
    }

    QWidget::show();
}

void MainWindow::startApp()
{
    configUI.show();
}


void MainWindow::on_startPushButton_clicked()
{
    ui->coilGroupBox->setEnabled(measRunning);
    ui->sampleGroupBox->setEnabled(measRunning);
    measRunning = !measRunning;
    if (measRunning)
        ui->startPushButton->setText("Stop");
    else
        ui->startPushButton->setText("Stop");
}
