fprintf('Testing variable operations... ');

utester = matshare.tests.single.unaryvaroptester;
btester = matshare.tests.single.binaryvaroptester;

% abs
utester.mshf = @abs;
utester.matf = @abs;

utester.test(-95);
utester.test(95);
utester.test(0);
utester.test(int8(-128));
utester.test(int8(-127));

% add
btester.mshf = @add;
btester.matf = @plus;

btester.test(2, 1);
btester.test(realmax, realmax);
btester.test(5, -7);
btester.test(-7, -5);
btester.test(-realmax, realmax);
btester.test(int8(-5), int8(5));
btester.test(int8(125), int8(25));
btester.test(int8(-125), int8(25));
btester.test(int8(-125), int8(-25));
btester.test(int8(-125), -25);
btester.test(uint8(225), uint8(100));

% sub
btester.mshf = @sub;
btester.matf = @minus;

btester.test(2, 1);
btester.test(realmax, realmax);
btester.test(5, -7);
btester.test(-7, -5);
btester.test(-realmax, realmax);
btester.test(int8(-5), int8(5));
btester.test(int8(125), int8(25));
btester.test(int8(-125), int8(25));
btester.test(int8(-125), int8(-25));
btester.test(int8(-125), -25);
btester.test(uint8(225), uint8(100));
btester.test(uint32(234), uint32(2384293));
btester.test(uint64(283749), uint64(234234234324234));

% mul
btester.mshf = @mul;
btester.matf = @times;

btester.test(2, 1);
btester.test(realmax, realmax);
btester.test(5, -7);
btester.test(-7, -5);
btester.test(-realmax, realmax);
btester.test(int8(-5), int8(5));
btester.test(int8(125), int8(25));
btester.test(int8(-125), int8(25));
btester.test(int8(-125), int8(-25));
btester.test(int8(-125), -25);
btester.test(uint8(225), uint8(100));
btester.test(uint32(234), uint32(2384293));
btester.test(uint64(283749), uint64(234234234324234));

% NOTE: int*double will not create the exact same result as MATLAB.


% div
btester.mshf = @div;
btester.matf = @rdivide;

btester.test(2, 1);
btester.test(realmax, realmax);
btester.test(5, -7);
btester.test(-7, -5);
btester.test(-realmax, realmax);
btester.test(int8(-5), int8(5));
btester.test(int8(125), int8(25));
btester.test(int8(-125), int8(25));
btester.test(int8(-125), int8(-25));
btester.test(int8(-125), -25);
btester.test(uint8(225), uint8(100));
btester.test(uint32(234), uint32(2384293));
btester.test(uint64(283749), uint64(234234234324234));
btester.test(uint64(283749), 423423432423);

% rem
btester.mshf = @rem;
btester.matf = @rem;

btester.test(2, 1);
btester.test(realmax, realmax);
btester.test(5, -7);
btester.test(-7, -5);
btester.test(-realmax, realmax);
btester.test(int8(-5), int8(5));
btester.test(int8(125), int8(25));
btester.test(int8(-125), int8(25));
btester.test(int8(-125), int8(-25));
btester.test(int8(-125), -25);
btester.test(uint8(225), uint8(100));
btester.test(uint32(234), uint32(2384293));
btester.test(uint64(283749), uint64(234234234324234));
btester.test(uint64(283749), 423423432423);

% mod
btester.mshf = @mod;
btester.matf = @mod;

btester.test(2, 1);
btester.test(realmax, realmax);
btester.test(5, -7);
btester.test(-7, -5);
btester.test(-realmax, realmax);
btester.test(int8(-5), int8(5));
btester.test(int8(125), int8(25));
btester.test(int8(-125), int8(25));
btester.test(int8(125), int8(-25));
btester.test(int8(-125), int8(-25));
btester.test(int8(-125), int8(0));
btester.test(int8(0), int8(-25));
btester.test(int8(0), int8(0));
btester.test(int8(-125), -25);
btester.test(uint8(225), uint8(100));
btester.test(uint32(234), uint32(2384293));
btester.test(uint64(283749), uint64(234234234324234));
btester.test(uint64(283749), 423423432423);
btester.test(125, 25);
btester.test(-125, 25);
btester.test(125, -25);
btester.test(-125, -25);
btester.test(-125, 0);
btester.test(0, -25);
btester.test(0, 0);

% neg
utester.mshf = @neg;
utester.matf = @uminus;

utester.test(-95);
utester.test(95);
utester.test(0);
utester.test(int8(-128));
utester.test(int8(-127));

% ars
btester.mshf = @ars;
btester.matf = @bitsra;

btester.test(2, 1);
btester.test(realmax, realmax);
btester.test(-realmax, realmax);
btester.test(int8(-5), int8(5));
btester.test(int8(125), int8(25));
btester.test(int8(-125), int8(25));
btester.test(int8(-125), int8(0));
btester.test(int8(0), int8(0));
btester.test(uint8(225), uint8(100));
btester.test(125, 25);
btester.test(-125, 25);
btester.test(-125, 0);
btester.test(0, 0);

% als
btester.mshf = @als;
btester.matf = @(x1,x2) x1.*2.^x2;

btester.test(2, 1);
btester.test(realmax, realmax);
btester.test(5, -7);
btester.test(-7, -5);
btester.test(-realmax, realmax);

fprintf('Test successful.\n\n');
