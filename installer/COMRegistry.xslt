<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:wix="http://schemas.microsoft.com/wix/2006/wi"
    xmlns="http://schemas.microsoft.com/wix/2006/wi"
    version="1.0"
    exclude-result-prefixes="xsl wix">

  <xsl:output method="xml" indent="yes" omit-xml-declaration="yes" />

  <xsl:strip-space elements="*" />

  <xsl:template match="/">
    <xsl:comment> This file is generated - do not modify! </xsl:comment>
    <xsl:apply-templates select="@*|node()"/>
  </xsl:template>

  <xsl:include href="COMRegistryTransform.xsl"/>
  <xsl:template match="@*|node()">
    <xsl:call-template name="com-reg-transform">
      <xsl:with-param name="file-id" select="'FITSExplorerExtension.dll'" />
      <xsl:with-param name="component-id" select="'COMRegistryComponent'" />
      <xsl:with-param name="component-guid" select="'9AC82F77-FEF6-4079-B2D6-BC62FD79E6D2'" />
    </xsl:call-template>
  </xsl:template>

</xsl:stylesheet>