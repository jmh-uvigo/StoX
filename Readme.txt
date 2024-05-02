
StoX: Stochastic multistage recruitment model for seed dispersal effectiveness
=====--------------------------------------------------------------------------

Seed dispersal effectiveness measures the number of new plants effectively produced by the services of seed disperser agents. This depends on a complex process involving multiple stages and actors, and has profound implications for conservation. StoX is a distribution agnostic multistage stochastic model that differentiates among dispersers in their contribution to seed rain and recruitment. It can be parameterized with quantity and quality components of dispersal measured in the field. It preserves the inherent stochastic nature of the recruitment process and can be validated by statistical comparison between its predictions and recruitment patterns in the field. StoX has already been used in several successful studies, at both population and community levels.

--------------------------------------------------------------------------------------------------------------------------


There is a binary executable for Windows ready to run in the win64 folder. Check the user manual in the doc folder.


--------------------------------------------------------------------------------------------------------------------------


Note for developers:

StoX is written in C++ and uses the Qt6 libraries for multiplatform development. 

1. You can download the open source version of Qt from:

https://www.qt.io/download-open-source

You can download the source code of the Qt libraries and compile them from scratch, or you can download the binaries for your system (Linux, Windows, MacOS), which is much quicker and simpler.

2. Run QtCreator, the RAD environment for developing Qt-based applications.

3. Click Open Project, go to the StoX source code folder, and open the CMakeLists file.

4. Select the appropriate Qt kit for your system, and you are ready to compile StoX.


