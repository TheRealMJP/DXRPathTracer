import maya.cmds as cmds

def connectTex(material):
    conn = cmds.listConnections(material+'.specularColor',type='file')
    if not conn:
        return

    color = cmds.getAttr(conn[0] + '.fileTextureName')
    roughness = color[:color.rfind('SR.dds')] + 'R.dds'

    # print roughness

    if not os.path.exists(roughness):
        print 'roughness doesnt exist'
        return

    print conn

    print material+'.specularColor'
    cmds.disconnectAttr(conn[0] + '.outColor', material+'.specularColor')

    input = "cosinePower"
    image = roughness

    # if a file texture is already connected to this input, update it
    # otherwise, delete it
    conn = cmds.listConnections(material+'.'+input,type='file')
    if conn:
        # there is a file texture connected. replace it
        # cmds.setAttr(conn[0]+'.fileTextureName',image,type='string')
        print "already connected"
    else:
        # no connected file texture, so make a new one
        newFile = cmds.shadingNode('file',asTexture=1)
        newPlacer = cmds.shadingNode('place2dTexture',asUtility=1)
        # make common connections between place2dTexture and file texture
        connections = ['rotateUV','offset','noiseUV','vertexCameraOne','vertexUvThree','vertexUvTwo','vertexUvOne','repeatUV','wrapV','wrapU','stagger','mirrorU','mirrorV','rotateFrame','translateFrame','coverage']
        cmds.connectAttr(newPlacer+'.outUV',newFile+'.uvCoord')
        cmds.connectAttr(newPlacer+'.outUvFilterSize',newFile+'.uvFilterSize')
        for i in connections:
            cmds.connectAttr(newPlacer+'.'+i,newFile+'.'+i)
        # now connect the file texture output to the material input
        cmds.connectAttr(newFile+'.outAlpha',material+'.'+input,f=1)
        # now set attributes on the file node.
        cmds.setAttr(newFile+'.fileTextureName',image,type='string')
        cmds.setAttr(newFile+'.filterType',0)

selected = cmds.ls(sl=True,long=True) or []
for eachSel in selected:
    connectTex(eachSel)
