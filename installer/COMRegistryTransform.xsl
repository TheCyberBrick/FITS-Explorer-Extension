<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:wix="http://schemas.microsoft.com/wix/2006/wi"
    xmlns="http://schemas.microsoft.com/wix/2006/wi"
    version="1.0"
    exclude-result-prefixes="xsl wix">

  <xsl:template name="com-reg-transform-replace-string">
    <xsl:param name="source" />
    <xsl:param name="find" />
    <xsl:param name="repl" />
    <xsl:choose>
      <xsl:when test="$source = '' or $find = '' or not($find)" >
        <xsl:value-of select="$source" />
      </xsl:when>
      <xsl:when test="contains($source, $find)">
        <xsl:value-of select="substring-before($source, $find)" />
        <xsl:value-of select="$repl" />
        <xsl:call-template name="com-reg-transform-replace-string">
          <xsl:with-param name="source" select="substring-after($source, $find)" />
          <xsl:with-param name="find" select="$find" />
          <xsl:with-param name="repl" select="$repl" />
        </xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$source" />
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="com-reg-transform">
    
    <xsl:param name="file-id" />
    <xsl:param name="component-id" />
    <xsl:param name="component-guid" />

    <!-- find generated file id -->
    <xsl:variable name="generated-file-id">
      <xsl:value-of select="//wix:Component/wix:File/@Id"/>
    </xsl:variable>
    
    <xsl:choose>
      
      <!-- replace generated file id with specified file id -->
      <xsl:when test="parent::*/@*[contains(., concat('[!', $generated-file-id, ']'))]">
        <xsl:attribute name="{name(.)}">
          <xsl:call-template name="com-reg-transform-replace-string">
            <xsl:with-param name="source" select="." />
            <xsl:with-param name="find" select="concat('[!', $generated-file-id, ']')" />
            <xsl:with-param name="repl" select="concat('[!', $file-id, ']')" />
          </xsl:call-template>
        </xsl:attribute>
      </xsl:when>
      
      <!-- replace generated component id with specified component id and strip generated file element-->
      <xsl:when test="self::wix:Component">
        <xsl:copy>
          <xsl:attribute name="Id">
            <xsl:value-of select="$component-id"/>
          </xsl:attribute>
          <xsl:attribute name="Guid">
            <xsl:value-of select="$component-guid"/>
          </xsl:attribute>
          <xsl:apply-templates select="@*[name()!='Id' and name()!='Guid']|node()[name()!='File']"/>
        </xsl:copy>
      </xsl:when>
      
      <!-- no other changes -->
      <xsl:otherwise>
        <xsl:copy>
          <xsl:apply-templates select="@*|node()"/>
        </xsl:copy>
      </xsl:otherwise>
      
    </xsl:choose>
    
  </xsl:template>

</xsl:stylesheet>