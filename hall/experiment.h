#ifndef EXPERIMENT_H
#define EXPERIMENT_H

#include <msdp2xxx.h>
#include <QObject>
#include <QScriptEngine>

#include "config.h"
#include "halldata.h"
#include "powpolswitch.h"
#include "unitconv.h"
#include "../QCSVWriter/qcsvwriter.h"
#include "../QSCPIDev/qscpidev.h"

class Experiment : public QObject
{
    Q_OBJECT
protected:
    /** Comprises one timed task of measurement automation. */
    typedef struct {
        /** Function to call. */
        void (*func)(Experiment *this_);
        /** Delay before go to next step. */
        int delay;
    } Step_t;

    /** Series of measurement automation steps. */
    class Steps_t : public QVector<Step_t>
    {
    public:
        Steps_t() : QVector<Step_t>() {}
        /** Initiate steps from array of steps => <begin, end).
            @par begin pointer to first element of array.
            @par end pointer behind last element of array.  */
        Steps_t(const Step_t *begin, const Step_t *end);
    };

    /** Array of steps for single "hand made" measurement. */
    static const Step_t stepsMeasure[];

    /** Vector of steps to run, created from autoSteps. */
    Steps_t stepsRunning;

    /** Current step of automated Hall measurement. */
    Steps_t::const_iterator stepCurrent;

    /** Used to evaluate user equations (B from hall probe U) */
    QScriptEngine scriptEngine;

    HallData hallData;
    HallData::MeasuredData measuredData;

public:
    /** Indexes of columns in CSV file with data from experiment. */
    enum {
        csvColNotes = 0,
        csvColHallProbeB,
        csvColSampleResistivity,
        csvColSampleResSpec,
        csvColSampleRHall,
        csvColSampleDrift,
        csvColSamplecCarrier,
        csvColResultsEnd,

        csvColTime,
        csvColHallProbeU,
        csvColSampleI,
        csvColSampleUac,
        csvColSampleUacRev,
        csvColSampleUbd,
        csvColSampleUbdRev,
        csvColSampleUcd,
        csvColSampleUcdRev,
        csvColSampleUda,
        csvColSampleUdaRev,
        csvColCoilI,

        /** csvColEnd equeals to a number of columns, it is not a column at all. */
        csvColEnd
    };

    /** Channel offset for 34901A card */
    static const int _34901A;
    /** 34901A: sample pins cd */
    static const int _34901A_sample_cd;
    /** 34901A: sample pins da */
    static const int _34901A_sample_da;
    /** 34901A: sample pins bd */
    static const int _34901A_sample_bd;
    /** 34901A: sample pins ac */
    static const int _34901A_sample_ac;
    /** 34901A: hall probe */
    static const int _34901A_hall_probe;

    /** Channel offset for 34903A card */
    static const int _34903A;

    static const int _34903A_min;
    static const int _34903A_max;

    /** 34903A: sample pin a <-> current source (-) */
    static const int _34903A_sample_a_pwr_m;
    /** 34903A: sample pin b <-> current source (+) */
    static const int _34903A_sample_b_pwr_p;
    /** 34903A: sample pin c <-> current source (0/(+/-)) */
    static const int _34903A_sample_c_pwr_sw1;
    /** 34903A: sample pin d <-> current source (-) */
    static const int _34903A_sample_d_pwr_m;
    /** 34903A: current source (0/(+/-)) <-> current source (-) */
    static const int _34903A_pwr_sw1_pwr_m;
    /** 34903A: current source (0/(+/-)) <-> current source (+) */
    static const int _34903A_pwr_sw1_pwr_p;
    /** 34903A: hall probe - pin 1 <-> current source (-) */
    static const int _34903A_hall_probe_1_pwr_m;
    /** 34903A: hall probe - pin 2 <-> current source (+) */
    static const int _34903A_hall_probe_2_pwr_p;

    explicit Experiment(Config *config, QObject *parent = 0);

    /** Check all set values.
      *
      * @return true if all requred values are set properly, false otherwise.
      */
    bool checkSettings();
    void close();
    const HallData &data() const;
    bool open();

    double coilI();
    double coilMaxI();
    const QVector<double> &getDataB();
    const QVector<double> &getDataHallU();
    const QVector<double> &getDataResistivity();
    int repeats();
    void setCoilI(double value);
    void setCoilIRange(double val1, double val2);
    void setRepeats(int repeats);

    bool isMeasuring();

    /** Start single or multiple measurements for defined coilWantI (<min, max>). */
    void measure(bool single = true);

    /** Stop measurement imediately. */
    void measurementAbort();

    /** Reset experiment status, clear all cached data. */
    bool reset();
    bool writeHeader();
    bool writeSummary(const QString &note = QString(""));

    QString HP34970Port();
    void setHP34970Port(QString port);
    QString PS6220Port();
    void setPS6220Port(QString port);
    QString MSDPPort();
    void setMSDPPort(QString port);

protected:
    /** File to save measured data. */
    QCSVWriter csvFile;
    /** Step of I on coil used for automated measurement. */
    double _coilIStep_;
    /** Timer used to adjust current trought magnet in specified time. */
    QTimer coilTimer;
    /** Wanted value of curr flowing trought coil. */
    double _coilWantI_;
    /** Delay betwen current value update [ms].
    *   BAD CODE INSIDE, DO NOT CHANGE! */
    static const int currentDwell = 1000;
    /** Slope of current change flowing trought magnet [A/sec]. */
    static const float currentSlope = 0.01;
    /** Power source output current limit. */
    float currentMax;
    /** HP 34970A device to measure voltage and resistivity. */
    QSCPIDev hp34970Dev;
    /** Timer used for fully automated testing process. */
    QTimer measTimer;
    /** Indicate whatever measurement is in progress. */
    bool _measuring_;
    /** Last set value of I on coil, used only for control purposes. */
    QList<double> _measuringRange_;
    /** Keithlay PS 6220 hacky class. */
    QSCPIDev ps6220Dev;
    /** Power polarity switch handler. */
    PwrPolSwitch pwrPolSwitch;
    /** Mansons SDP power supply driver. */
    sdp_t sdp;
    /** True when sweepeng tovards wanted U value on coil. */
    bool _sweeping_;

    /** Compute B from U on hall probe. */
    double computeB(double I, double U);
    /** Read single value from 34901A. */
    double readSingle();

    /* Steps for Hall measurement automation */
    /** Abort process. */
    static void stepAbort(Experiment *this_);
    /** Abort measurement if target coil I reached */
    static void stepAbortIfTargetReached(Experiment *this_);
    /** Finish measurement, write data into file etc. . */
    static void stepFinish(Experiment *this_);
    /** Get current time and put in into measurement data. */
    static void stepGetTime(Experiment *this_);
    /** Prepare measurement on hall probe. */
    static void stepMeasHallProbePrepare(Experiment *this_);
    /** Do measurement on hall probe. */
    static void stepMeasHallProbe(Experiment *this_);
    /** Restart running measurement from begin. */
    static void stepRestart(Experiment *this_);

    static void stepSampleMeas_ac(Experiment *this_);
    static void stepSampleMeas_acRev(Experiment *this_);
    static void stepSampleMeas_bd(Experiment *this_);
    static void stepSampleMeas_bdRev(Experiment *this_);
    static void stepSampleMeas_cd(Experiment *this_);
    static void stepSampleMeas_cdRev(Experiment *this_);
    static void stepSampleMeas_da(Experiment *this_);
    static void stepSampleMeas_daRev(Experiment *this_);
    static void stepSampleMeasPrepare_cd(Experiment *this_);
    static void stepSampleMeasPrepare_da(Experiment *this_);
    static void stepSampleMeasPrepare_ac(Experiment *this_);
    static void stepSampleMeasPrepare_bd(Experiment *this_);
    /** Put power on sample on pins (b, a) = (+, -) */
    static void stepSamplePower_ba(Experiment *this_);
    /** Put power on sample on pins (b, c) = (+, -) */
    static void stepSamplePower_bc(Experiment *this_);
    /** Put power on sample on pins (b, d) = (+, -) */
    static void stepSamplePower_bd(Experiment *this_);
    /** Put power on sample on pins (c, a) = (+, -) */
    static void stepSamplePower_ca(Experiment *this_);
    /** Set sample power source power. */
    static void stepSamplePower_mp(Experiment *this_);
    /** Set sample power source power (reversed). */
    static void stepSamplePower_pm(Experiment *this_);
    /** Set new coil target I, this I value will be put on coil in next round. */
    static void stepSetNewTarget(Experiment *this_);
    /** Wait until coil I is not set to wanted value */
    static void stepSweeping(Experiment *this_);

signals:
    void coilBMeasured(double B);
    void coilIMeasured(double I);
    void coilUMeasured(double U);
    /** Emited when fatal error in experiment occured.
      *
      * State of experiment is undefined when this signal is emited.
      * Only close() might/should/must be called. This signal should be handled even
      * before open and might be emited asynchronously by timmed operations.
      */
    void fatalError(const QString &errorShort, const QString &errorLong);
    void measured(
        const HallData::MeasuredData &measuredData,
        const HallData::EvaluatedData &evaluatedData,
        const HallData::SummaryData &summaryData);
    void measurementCompleted();
    void sweepingCompleted();

private slots:
    void on_coilTimer_timeout();
    void on_measTimer_timeout();
    void on_hallData_measurementAcquired(
        const HallData::MeasuredData &measuredData,
        HallData::EvaluatedData& evaluatedData);
    void on_hallData_measurementAdded(
        const HallData::MeasuredData &measuredData,
        const HallData::EvaluatedData &evaluatedData);

private:
    /** Script to compute intensity of B from hall probe U and I. */
    QString _equationB_;
    static const QString eqationBScript;
    double _coilIRangeBottom_, _coilIRangeTop_;
    double _coilMaxI_;
    Config *config;

    /** I for hall probe to measure B. */
    double hallProbeI;

    bool needHeader;

    /** Elementar charge, charge of single electron. */
    static const double q;

    /** Number of automatic measurement cycles. */
    int _repeats_;
    double _sampleThickness_;

    /** Create path to new file to store measurement data. */
    QString filePath();
    static std::pair<double, double> linRegress(const QVector<double> &x,
                                         const QVector<double> &y);
};

#endif // EXPERIMENT_H
