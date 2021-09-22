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

  <xsl:include href="RegistryHarvestTransform.xsl"/>
  <xsl:template match="@*|node()">
    <xsl:call-template name="reg-harvest-transform">
      <xsl:with-param name="file-id" select="'FITSExplorerExtension.dll'" />
      <xsl:with-param name="hkcu-component-id" select="'COMRegistryComponentHKCU'" />
      <xsl:with-param name="hkcu-component-guid" select="'9AC82F77-FEF6-4079-B2D6-BC62FD79E6D2'" />
      <xsl:with-param name="hkcu-keypath-position" select="7" />
      <xsl:with-param name="hklm-component-id" select="'COMRegistryComponentHKLM'" />
      <xsl:with-param name="hklm-component-guid" select="'F0712DE2-7B4B-4BAD-A32E-F72DC5C7A724'" />
      <xsl:with-param name="hklm-keypath-position" select="2" />
    </xsl:call-template>
  </xsl:template>

</xsl:stylesheet>