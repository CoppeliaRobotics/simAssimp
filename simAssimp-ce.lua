local codeEditorInfos = [[
simAssimp.exportMeshes(map allVertices, map allIndices, string filename, string formatId, float scaling=1.0, int upVector=sim_assimp_upvect_z, int options=0)\n\nExports the specified mesh data.
simAssimp.exportShapes(int[] shapeHandles, string filename, string formatId, float scaling=1.0, int upVector=sim_assimp_upvect_z, int options=0)\n\nExports the specified shapes. Depending on the fileformat, several files will be created (e.g. myFile.obj, myFile.mtl, myFile_2180010.png, etc.)
simAssimp.exportShapesDlg(string filename, int[] shapeHandles)\n\nOffers export parameters via dialog, before calling simAssimp.export
string formatDescription, string formatExtension, string formatId = simAssimp.getExportFormat(int index)\n\nAllows to loop through supported file formats for export
string formatDescription, string formatExtension = simAssimp.getImportFormat(int index)\n\nAllows to loop through supported file formats for import
map allVertices, map allIndices = simAssimp.importMeshes(string filenames, float scaling=0.0, int upVector=sim_assimp_upvect_auto, int options=0)\n\nImports the specified files as mesh data
int[] shapeHandles = simAssimp.importShapes(string filenames, int maxTextureSize=512, float scaling=0.0, int upVector=sim_assimp_upvect_auto, int options=0)\n\nImports the specified files as shapes
int[] handles = simAssimp.importShapesDlg(string filename)\n\nOffers import parameters via dialog, before calling simAssimp.import
simAssimp.upVector
simAssimp.upVector.auto
simAssimp.upVector.y
simAssimp.upVector.z
]]

registerCodeEditorInfos("simAssimp", codeEditorInfos)
