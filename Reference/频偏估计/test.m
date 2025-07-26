clear; clc;
Fs = 5e5;                           % 采样频率
Ts = 1 / Fs;
SignalLenSecond = 1e-2;             % 信号时长
NumSamp = round(SignalLenSecond * Fs);
IndexSamp = 0 : NumSamp - 1;

FreqEstMethod = 1;                  % 0 -- Rife; 1 -- Quinn
NumSimu = 1000;

FineFreqEstErr = zeros(1, NumSimu);
CoarseFreqEstErr = zeros(1, NumSimu);
FreqCollect = zeros(1, NumSimu);
DeltaCollect = zeros(1, NumSimu);

for iSimu = 1:NumSimu
    InitialPhase = rand(1, 1) * pi * 2;
    Freq = randi([300, 7000], 1, 1);
    FreqCollect(iSimu) = Freq;
    
    Signal = exp(1i * 2 * pi * Freq * IndexSamp * Ts + 1i * InitialPhase);
    SignalFD = fft(Signal);
    [MaxValue, MaxValueLoc] = max(abs(SignalFD));
    
    CoarseFreqEst = (MaxValueLoc - 1) / NumSamp * Fs;
    
    if 0 == FreqEstMethod
        DeltaNumerator = abs(SignalFD(MaxValueLoc + 1)) - abs(SignalFD(MaxValueLoc - 1));
        DeltadeNominator = abs(SignalFD(MaxValueLoc)) + abs(SignalFD(MaxValueLoc + 1));
        Delta = (DeltaNumerator) / (DeltadeNominator);
        
        FinalFreqEst = (MaxValueLoc + Delta / 2 - 1) / NumSamp * Fs;
        
        FineFreqEstErr(iSimu) = FinalFreqEst - Freq;
        CoarseFreqEstErr(iSimu) = CoarseFreqEst - Freq;
        
        FineStd = std(FineFreqEstErr);
        CoarseStd = std(CoarseFreqEstErr);
        
    elseif 1 == FreqEstMethod
        Beta_1 = real(SignalFD(MaxValueLoc - 1) / SignalFD(MaxValueLoc));
        Delta_1 = Beta_1 / (1 - Beta_1);
        
        Beta_2 = real(SignalFD(MaxValueLoc + 1) / SignalFD(MaxValueLoc));
        Delta_2 = Beta_2 / (Beta_2 - 1);
        
        if Delta_1 > 0 && Delta_2 > 0
            Delta = Delta_2;
        else
            Delta = Delta_1;
        end
        
        DeltaCollect(iSimu) = Delta;
        
        FinalFreqEst = (MaxValueLoc + Delta - 1) / NumSamp * Fs;
        
        FineFreqEstErr(iSimu) = FinalFreqEst - Freq;
        CoarseFreqEstErr(iSimu) = CoarseFreqEst - Freq;
    end
end

plot(CoarseFreqEstErr);
hold on;
plot(FineFreqEstErr);
grid on;
title('无噪声时频偏估计误差');
legend('FFT频偏估计算法', '新频偏估计算法');