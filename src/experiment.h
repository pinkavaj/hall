#ifndef EXPERIMENT_H
#define EXPERIMENT_H

#include <QObject>
#include <msdp2xxx.h>

#include "config.h"
#include "hp34970hack.h"
#include "powpolswitch.h"
#include "ps6220hack.h"
#include "qcsvfile.h"

class Experiment : public QObject
{
    Q_OBJECT
protected:
    /** Comprises one timed task in measurement automation. */
    typedef struct {
        /** Function to call.
            @return true to continue measurement, false to abort measurement. */
        void (*func)(Experiment *this_);
        /** Delay before next step execution. */
        int delay;
    } Step_t;

    /** Series of measurement automation steps. */
    class Steps_t : public QVector<Step_t>
    {
    public:
        Steps_t() : QVector<Step_t>() {};
        /** Initiate steps from array of steps in form <begin, end).
            @par begin pointer to first element of array.
            @par end pointer behind last element of array.  */
        Steps_t(const Step_t *begin, const Step_t *end);
    };

    /** Array of steps for fully automatized Hall measurement. */
    static const Step_t stepsAll[];
    /** Array of steps for single "hand made" measurement. */
    static const Step_t stepsMeasure[];
    /** Vector of steps to run, created from autoSteps. */
    Steps_t stepsRunning;
    /** Current step of automated Hall measurement. */
    Steps_t::const_iterator stepCurrent;

public:
    /** Indexes of columns in CSV file with data from experiment. */
    enum {
        csvColTime = 0,
        csvColHallProbeI,
        csvColHallProbeU,
        csvColSampleI,
        csvColSampleUacF,
        csvColSampleUacB,
        csvColSampleUbdF,
        csvColSampleUbdB,
        csvColSampleUcdF,
        csvColSampleUcdB,
        csvColSampleUdaF,
        csvColSampleUdaB,
        csvColEmpty,
        csvColHallProbeB,
        csvColSampleHallU,
        csvColSampleResistivity,
        /** csvColEnd equeals to a number of columns, it is not a column at all. */
        csvColEnd,
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

    explicit Experiment(QObject *parent = 0);

    /** Close all devices, eg. power supply, Agilent, switch, ... */
    void close();
    void open();

    void setCoefficients(double B1, double B2, double B3);
    double coefficientB1();
    double coefficientB2();
    double coefficientB3();

    double coilI();
    double coilMaxI();
    void setCoilI(double value);
    void setCoilIRange(double value);

    double sampleI();
    void setSampleI(double value);

    bool isMeasuring();
    /** Start single or multiple measurements for defined coilWantI (<min, max>). */
    void measure(bool single = true);
    /** Stop measurement imediately. */
    void measurementStop();

    QString HP34970Port();
    void setHP34970Port(QString port);
    QString PS6220Port();
    void setPS6220Port(QString port);
    QString MSDPPort();
    void setMSDPPort(QString port);

protected:
    /** File to save measured data. */
    QCSVFile csvFile;
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
    HP34970Hack hp34970Hack;
    /** Timer used for fully automated testing process. */
    QTimer measTimer;
    /** Indicate whatever measurement is in progress. */
    bool _measuring_;
    /** Keithlay PS 6220 hacky class. */
    PS6220Hack ps622Hack;
    /** Power polarity switch handler. */
    PwrPolSwitch pwrPolSwitch;
    /** Mansons SDP power supply driver. */
    sdp_t sdp;
    /** True when sweepeng tovards wanted U value on coil. */
    bool _sweeping_;

    /** Use constants B1, B2 and B3 and compute B from U on hall probe. */
    double computeB(double U);
    /** Read single value from 34901A. */
    double readSingle();

    /* Steps for Hall measurement automation */
    /** Abort process. */
    static void stepAbort(Experiment *this_);
    /** Finish measurement, write data into file etc. . */
    static void stepFinish(Experiment *this_);
    /** Get current time and put in into measurement data. */
    static void stepGetTime(Experiment *this_);
    /** Open all routes (clean up). */
    static void stepOpenAllRoutes(Experiment *this_);
    /** Prepare measurement on hall probe. */
    static void stepMeasHallProbePrepare(Experiment *this_);
    /** Do measurement on hall probe. */
    static void stepMeasHallProbe(Experiment *this_);

    static void stepSampleMeas_cd(Experiment *this_);
    static void stepSampleMeas_cdRev(Experiment *this_);
    static void stepSampleMeas_da(Experiment *this_);
    static void stepSampleMeas_daRev(Experiment *this_);
    static void stepSampleMeas_ac(Experiment *this_);
    static void stepSampleMeas_acRev(Experiment *this_);
    static void stepSampleMeas_bd(Experiment *this_);
    static void stepSampleMeas_bdRev(Experiment *this_);
    /** Prepare measurement on sample pins c, d. */
    static void stepSampleMeasPrepare_cd(Experiment *this_);
    /** Prepare measurement on sample pins d, a. */
    static void stepSampleMeasPrepare_da(Experiment *this_);
    /** Prepare measurement on sample pins a, c. */
    static void stepSampleMeasPrepare_ac(Experiment *this_);
    /** Prepare measurement on sample pins b, d. */
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

signals:
    void measured(QString time, double B, double hallU, double resistivity);
    void measurementCompleted();
    void sweepingCompleted();
    void coilBMeasured(double B);
    void coilIMeasured(double I);
    void coilUMeasured(double U);

public slots:
private slots:
    void on_coilTimer_timeout();
    void on_measTimer_timeout();

private:
    double B1, B2, B3;
    double _coilMaxI_;
    Config config;
    double dataUcd, dataUdc, dataUda, dataUad;
    double dataUac, dataUca, dataUbd, dataUdb;
    double _dataB_, _dataHallU_, _dataResistivity_;
    /** I for hall probe to measure B. */
    static const double hallProbeI;
    double _sampleI_;
};

#endif // EXPERIMENT_H
